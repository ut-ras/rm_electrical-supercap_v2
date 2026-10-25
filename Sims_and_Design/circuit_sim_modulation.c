#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#define PI 3.14159265358979323846
#define DELAY_AMOUNT 30  // Sensor delay in circuit timesteps.

// Change simulation settings here, then build and run.
static const double C = 440.0;       // Capacitance (F).
static const double R = 0.0095;      // Resistance (ohms).
static const double L = 28.3e-6;     // Inductance (H).
static const double Vg = 24.0;       // Source voltage (V).
static const double vc_initial = 2.5;
static const double i_initial = 0.0;
static const double freq_HZ = 100.0;
static const double current_amplitude = 2.5;
static const double i_limit = 30.0;
static const double current_margin = 1.0;
static const double controller_update_Hz = 12000.0;
static const double controller_bandwidth_Hz = 300.0;
static const int controller_steps = 10;  // Circuit steps per controller update.
static const int modulation_controller_cycles = 2;
static const double total_time = 20.9;   // Seconds; 5 * R * C with these defaults.
static const uint32_t seed = 1;          // Change to use a different noise sequence.
static const int noise_enabled = 1;     // 1 = on, 0 = off.
static const int plot_enabled = 1;      // Uses plot_circuit.py when available.
static const char output_name[] = "circuit_data.csv";  // Also used by the plot script.

// Circuit state, controller state, and statistics are kept in separate structs.
typedef struct {
    double vc, i, m;  // Capacitor voltage, inductor current, and duty cycle.
    double input_energy, loss_energy;
} Circuit;

typedef struct {
    double input_1, input_2;
    double output_1, output_2;
    double input_gain, output_gain;
} BatteryFilter;

typedef struct {
    double period, kp, ki;
    double vg_sense, vc_sense, i_sense, ibat_sense, error;
    double ibat_sense_raw;
    double mt, mt_requested, integral, previous_error;
    double mt_min, mt_max;
    BatteryFilter battery_filter;
} Controller;

typedef struct {
    long long steps, metric_start, metric_count, modulation_count;
    long long sqrt_limit_count, duty_limit_count;
    long long current_limit_count, current_violation_count;
    double error_squared_sum, error_sum, ibat_sum, i_squared_sum;
    double max_error, peak_i, min_m, max_m, initial_energy;
} Statistics;

// Each sensor has its own repeatable noise sequence.
static uint32_t random_state[3];
static int have_extra_noise[3];
static double extra_noise[3];

// Generates random sensor noise with a normal distribution.
static double generate_gaussian(int sensor) {
    // Box-Muller gives two samples at once, so use the leftover one first.
    if (have_extra_noise[sensor]) {
        have_extra_noise[sensor] = 0;
        return extra_noise[sensor];
    }

    // Make two uniform random numbers using a small xorshift generator.
    double u[2];
    for (int j = 0; j < 2; j++) {
        uint32_t value = random_state[sensor];
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        random_state[sensor] = value;
        u[j] = ((double)value + 0.5) / 4294967296.0;
    }

    // Box-Muller produces two Gaussian samples; save one for the next call.
    double radius = sqrt(-2.0 * log(u[0]));
    double angle = 2.0 * PI * u[1];
    extra_noise[sensor] = radius * sin(angle);
    have_extra_noise[sensor] = 1;
    return radius * cos(angle);
}

// Calculates the energy currently stored in the capacitor and inductor.
static double stored_energy(const Circuit* circuit) {
    return 0.5 * C * (*circuit).vc * (*circuit).vc
        + 0.5 * L * (*circuit).i * (*circuit).i;
}

// Sets up the discrete form of H(s) = 1 / (L*C*s*s + 1).
// This is the proposed undamped LC model; it can ring near its natural frequency.
static int initialize_battery_filter(BatteryFilter* filter, double sample_period,
    double initial_current) {
    double t_squared = sample_period * sample_period;
    double denominator = 4.0 * L * C + t_squared;
    if (!isfinite(denominator) || denominator <= 0.0) return 0;

    (*filter).input_gain = t_squared / denominator;
    (*filter).output_gain = (8.0 * L * C - 2.0 * t_squared) / denominator;
    // A constant initial current is also the steady output, since DC gain is 1.
    (*filter).input_1 = initial_current;
    (*filter).input_2 = initial_current;
    (*filter).output_1 = initial_current;
    (*filter).output_2 = initial_current;
    return isfinite((*filter).input_gain) && isfinite((*filter).output_gain);
}

// Advances the filter once per controller update, not once per circuit step.
static double filtered_battery_current(BatteryFilter* filter, double input) {
    double output = (*filter).input_gain
        * (input + 2.0 * (*filter).input_1 + (*filter).input_2)
        + (*filter).output_gain * (*filter).output_1 - (*filter).output_2;

    // Calculate the output before moving history, so output_2 is still two samples old.
    (*filter).input_2 = (*filter).input_1;
    (*filter).input_1 = input;
    (*filter).output_2 = (*filter).output_1;
    (*filter).output_1 = output;
    return output;
}

// Checks the settings and sets up the circuit, controller, and statistics.
static int initialize_simulation(Circuit* circuit, Controller* controller, Statistics* stats) {
    if (!(C > 0.0 && R >= 0.0 && L > 0.0 && Vg > 0.0
        && controller_update_Hz > 0.0 && controller_bandwidth_Hz > 0.0
        && controller_steps > 0 && modulation_controller_cycles > 0
        && current_margin >= 0.0 && i_limit > current_margin)) {
        fprintf(stderr, "Invalid circuit parameters, update rates, or current limit.\n");
        return 0;
    }

    (*controller).period = 1.0 / controller_update_Hz;
    double timestep = (*controller).period / controller_steps;
    double step_count = total_time / timestep;
    if (!isfinite(step_count) || step_count < 1.0 || step_count > 1e12) {
        fprintf(stderr, "Duration must span 1 to 1e12 circuit steps.\n");
        return 0;
    }
    // Convert the requested run time into the number of small circuit steps.
    (*stats).steps = llround(step_count);
    // Calculate tracking statistics over the final second, or the whole run if shorter.
    long long one_second_steps = llround(1.0 / timestep);
    long long metric_steps = (*stats).steps < one_second_steps ? (*stats).steps : one_second_steps;
    (*stats).metric_start = (*stats).steps - metric_steps;

    // Choose PI gains from the requested controller bandwidth.
    double tau_i = 1.0 / (2.0 * PI * controller_bandwidth_Hz);
    (*controller).kp = L / (Vg * tau_i);
    (*controller).ki = R / (Vg * tau_i);
    if (!isfinite((*controller).kp) || !isfinite((*controller).ki)) {
        fprintf(stderr, "Controller gains are not finite.\n");
        return 0;
    }

    // Give each sensor a different noise sequence, even with the same seed.
    const uint32_t sensor_tags[3] = {
        UINT32_C(0xa341316c), UINT32_C(0xc8013ea4), UINT32_C(0xad90777d)
    };
    for (int sensor = 0; sensor < 3; sensor++) {
        random_state[sensor] = seed ^ sensor_tags[sensor];
        if (random_state[sensor] == 0) random_state[sensor] = UINT32_C(0x6d2b79f5);
        have_extra_noise[sensor] = 0;
    }

    (*circuit).vc = vc_initial;
    (*circuit).i = i_initial;
    // Start the duty cycle near the value that balances the capacitor voltage.
    (*circuit).m = fmax(0.0, fmin(1.0, (*circuit).vc / Vg));
    (*controller).vg_sense = Vg;
    (*controller).vc_sense = (*circuit).vc;
    (*controller).i_sense = (*circuit).i;
    (*controller).ibat_sense_raw = (*circuit).m * (*circuit).i;
    (*controller).ibat_sense = (*controller).ibat_sense_raw;
    // The filter runs at controller_update_Hz, so use the full controller period.
    if (!initialize_battery_filter(&(*controller).battery_filter, (*controller).period,
        (*controller).ibat_sense_raw)) {
        fprintf(stderr, "Invalid battery-current filter coefficients.\n");
        return 0;
    }
    (*controller).mt = (*circuit).m * (*circuit).m
        - ((*controller).vc_sense / (*controller).vg_sense) * (*circuit).m;
    (*controller).mt_requested = (*controller).mt;
    (*controller).integral = (*controller).mt;  // Integral contribution is in MT units.
    (*stats).peak_i = fabs((*circuit).i);
    (*stats).min_m = (*circuit).m;
    (*stats).max_m = (*circuit).m;
    (*stats).initial_energy = stored_energy(circuit);
    return 1;
}

// Uses delayed sensor readings to calculate the next PI control command.
static int update_controller(const Circuit* circuit, Controller* controller,
    double delayed_i, double delayed_i_ref, double current_time) {
    // Add measurement noise to the delayed readings, not the actual circuit state.
    double vg_noise = 0.0, i_noise = 0.0, i_ref_noise = 0.0;
    if (noise_enabled) {
        vg_noise = 0.0903 * generate_gaussian(0);
        i_noise = 0.141 * generate_gaussian(1);
        i_ref_noise = 0.0855 * generate_gaussian(1);
    }
    (*controller).vg_sense = Vg + vg_noise;
    (*controller).i_sense = delayed_i + i_noise;
    double i_ref_sense = delayed_i_ref + i_ref_noise;
    if (!isfinite((*controller).vg_sense) || (*controller).vg_sense <= 0.0) {
        fprintf(stderr, "Invalid sensed source voltage at %.9f s.\n", current_time);
        return 0;
    }

    // Filter the measured branch current; leave the physical circuit state alone.
    (*controller).ibat_sense_raw = (*circuit).m * (*controller).i_sense;
    (*controller).ibat_sense = filtered_battery_current(&(*controller).battery_filter,
        (*controller).ibat_sense_raw);
    if (!isfinite((*controller).ibat_sense)) {
        fprintf(stderr, "Non-finite filtered battery current at %.9f s.\n", current_time);
        return 0;
    }
    (*controller).error = i_ref_sense - (*controller).ibat_sense;

    // Feedforward uses the slope of the delayed sine wave to anticipate changes.
    // This approximation omits L * i * dm/dt.
    double timestep = (*controller).period / controller_steps;
    double delayed_time = current_time - timestep * DELAY_AMOUNT;
    double i_ref_derivative = current_amplitude * 2.0 * PI * freq_HZ
        * cos(2.0 * PI * freq_HZ * delayed_time);
    double feedforward = (L * i_ref_derivative + R * i_ref_sense) / (*controller).vg_sense;

    // Accumulate tracking error using trapezoidal integration between controller updates.
    (*controller).integral += 0.5 * (*controller).period * (*controller).ki
        * ((*controller).error + (*controller).previous_error);
    (*controller).mt_requested = (*controller).kp * (*controller).error
        + (*controller).integral + feedforward;
    (*controller).previous_error = (*controller).error;
    if (!isfinite((*controller).mt_requested)) {
        fprintf(stderr, "Non-finite controller command at %.9f s.\n", current_time);
        return 0;
    }
    return 1;
}

// Shared RK4 model, also used to predict forward from delayed sensor readings.
static int step_circuit(Circuit* circuit, double timestep, double source_voltage);

// Estimate the present state by replaying duties applied since the delayed sample.
static int predict_present_state(const Controller* controller,
    const double duty_history[DELAY_AMOUNT], long long n,
    double* present_vc, double* present_i) {
    Circuit prediction = { 0 };
    prediction.vc = (*controller).vc_sense;
    prediction.i = (*controller).i_sense;
    double timestep = (*controller).period / controller_steps;
    // Before the delay buffer fills, its initial readings represent time zero.
    int count = n < DELAY_AMOUNT ? (int)n : DELAY_AMOUNT;
    int first = n < DELAY_AMOUNT ? 0 : (int)(n % DELAY_AMOUNT);
    for (int j = 0; j < count; j++) {
        prediction.m = duty_history[(first + j) % DELAY_AMOUNT];
        if (!step_circuit(&prediction, timestep, (*controller).vg_sense)) return 0;
    }
    *present_vc = prediction.vc;
    *present_i = prediction.i;
    return 1;
}

// Limits the duty cycle so predicted inductor current stays within bounds.
static int apply_current_limit(Circuit* circuit, const Controller* controller,
    Statistics* stats, double proposed_m, double current_time,
    double predicted_vc, double predicted_i) {
    // Predict current over the time between duty updates.
    double h = (*controller).period * modulation_controller_cycles;
    double beta = R / 2.0 + h / (4.0 * C);
    // Leave some room below the hard current limit for prediction error.
    double control_limit = i_limit - current_margin;

    // Use the estimated present state, not the old sensor values, in the duty bounds.
    double m_min = (predicted_vc
        + (L / h) * (-control_limit - predicted_i)
        + beta * (-control_limit + predicted_i)) / (*controller).vg_sense;
    double m_max = (predicted_vc
        + (L / h) * (control_limit - predicted_i)
        + beta * (control_limit + predicted_i)) / (*controller).vg_sense;
    // The converter can only use duty cycles between 0 and 1.
    m_min = fmax(0.0, m_min);
    m_max = fmin(1.0, m_max);
    if (m_min > m_max) {
        fprintf(stderr, "No feasible current-limited duty at %.9f s.\n", current_time);
        return 0;
    }

    proposed_m = fmax(0.0, fmin(1.0, proposed_m));
    if (proposed_m < m_min || proposed_m > m_max) (*stats).current_limit_count++;
    (*circuit).m = fmax(m_min, fmin(m_max, proposed_m));
    return 1;
}

// Converts the controller command into a duty cycle and applies its limits.
static int update_modulation(Circuit* circuit, Controller* controller,
    Statistics* stats, double delayed_vc, double current_time,
    const double duty_history[DELAY_AMOUNT], long long n) {
    double vc_noise = noise_enabled ? 0.0103 * generate_gaussian(2) : 0.0;
    (*controller).vc_sense = delayed_vc + vc_noise;
    double kt = (*controller).vc_sense / (2.0 * (*controller).vg_sense);
    if (!isfinite(kt) || kt < 0.0 || kt >= 1.0) {
        fprintf(stderr, "Sensed Vc/Vg is outside the upper-root model range at %.9f s.\n",
            current_time);
        return 0;
    }

    // Solve MT = m^2 - 2*kt*m for m, using the upper quadratic root.
    (*controller).mt_min = -kt * kt;
    (*controller).mt_max = 1.0 - 2.0 * kt;
    double sqrt_term = (*controller).mt_requested + kt * kt;
    // Avoid taking the square root of a negative number if MT is out of range.
    double proposed_m = kt + sqrt(fmax(0.0, sqrt_term));
    if (sqrt_term < 0.0) (*stats).sqrt_limit_count++;
    if (proposed_m < 0.0 || proposed_m > 1.0) (*stats).duty_limit_count++;
    (*stats).modulation_count++;

    double predicted_vc, predicted_i;
    if (!predict_present_state(controller, duty_history, n, &predicted_vc, &predicted_i)) {
        fprintf(stderr, "Non-finite current-limit prediction at %.9f s.\n", current_time);
        return 0;
    }
    if (!apply_current_limit(circuit, controller, stats, proposed_m, current_time,
        predicted_vc, predicted_i)) return 0;

    // Prevent integral buildup when a limit changes the requested command.
    (*controller).mt = (*circuit).m * (*circuit).m - 2.0 * kt * (*circuit).m;
    (*controller).integral += (*controller).mt - (*controller).mt_requested;
    return 1;
}

// Advances the circuit one timestep using RK4 and updates energy totals.
static int step_circuit(Circuit* circuit, double timestep, double source_voltage) {
    double vc = (*circuit).vc, i = (*circuit).i, m = (*circuit).m;
    // Circuit equations: dVc/dt = I/C and dI/dt = (m*source_voltage - R*I - Vc)/L.
    double k1_vc = i / C;
    double k1_i = (m * source_voltage - R * i - vc) / L;

    // RK4 checks the slopes at two half-steps and one full step.
    double vc2 = vc + (0.5 * timestep) * k1_vc;
    double i2 = i + (0.5 * timestep) * k1_i;
    double k2_vc = i2 / C;
    double k2_i = (m * source_voltage - R * i2 - vc2) / L;

    double vc3 = vc + (0.5 * timestep) * k2_vc;
    double i3 = i + (0.5 * timestep) * k2_i;
    double k3_vc = i3 / C;
    double k3_i = (m * source_voltage - R * i3 - vc3) / L;

    double vc4 = vc + timestep * k3_vc;
    double i4 = i + timestep * k3_i;
    double k4_vc = i4 / C;
    double k4_i = (m * source_voltage - R * i4 - vc4) / L;

    // Use the same RK4 stage currents to integrate source and resistor energy.
    (*circuit).input_energy += timestep * source_voltage * m * (i + 2.0 * i2 + 2.0 * i3 + i4) / 6.0;
    (*circuit).loss_energy += timestep * R
        * (i * i + 2.0 * i2 * i2 + 2.0 * i3 * i3 + i4 * i4) / 6.0;
    // Combine the four slopes with RK4 weights to get the next state.
    (*circuit).vc += timestep * (k1_vc + 2.0 * k2_vc + 2.0 * k3_vc + k4_vc) / 6.0;
    (*circuit).i += timestep * (k1_i + 2.0 * k2_i + 2.0 * k3_i + k4_i) / 6.0;
    return isfinite((*circuit).vc) && isfinite((*circuit).i)
        && isfinite((*circuit).input_energy) && isfinite((*circuit).loss_energy);
}

// Opens the CSV file and writes its column headings.
static FILE* open_output(void) {
    FILE* output = fopen(output_name, "w");
    if (output == NULL) {
        perror("Cannot open CSV output");
        return NULL;
    }
    setvbuf(output, NULL, _IOFBF, 1024 * 1024);
    fprintf(output, "Time,Vc,I,Iref,Ibat,Error,Power,M,MT,MT_requested,"
        "Isense,Ibatsense,Vgsense,Vcsense,Error_sense,MT_min,MT_max,"
        "Sqrt_limits,Duty_limits,Energy_J,Input_energy_J,"
        "Loss_energy_J,Energy_residual_J,Ibat_sense_raw\n");
    return output;
}

// Updates performance statistics and periodically writes a CSV row.
static void record_results(FILE* output, const Circuit* circuit, const Controller* controller,
    Statistics* stats, long long n, double current_time, double i_ref) {
    // Ibat, Error and Power describe the physical circuit. Ibatsense and Error_sense
    // describe filtered feedback; logging must never advance the controller's filter.
    double ibat = (*circuit).m * (*circuit).i;
    double tracking_error = i_ref - ibat;
    (*stats).peak_i = fmax((*stats).peak_i, fabs((*circuit).i));
    (*stats).min_m = fmin((*stats).min_m, (*circuit).m);
    (*stats).max_m = fmax((*stats).max_m, (*circuit).m);
    if (fabs((*circuit).i) > i_limit + 1e-6) (*stats).current_violation_count++;
    // Only average tracking errors over the chosen final part of the run.
    if (n >= (*stats).metric_start && n < (*stats).steps) {
        (*stats).metric_count++;
        (*stats).error_squared_sum += tracking_error * tracking_error;
        (*stats).error_sum += tracking_error;
        (*stats).ibat_sum += ibat;
        (*stats).i_squared_sum += (*circuit).i * (*circuit).i;
        (*stats).max_error = fmax((*stats).max_error, fabs(tracking_error));
    }

    // Save one CSV row per controller update, plus the final sample.
    if (n % controller_steps != 0 && n != (*stats).steps) return;
    double energy = stored_energy(circuit);
    // Any leftover after stored energy = initial + input - losses is numerical error.
    double energy_residual = energy - (*stats).initial_energy
        - (*circuit).input_energy + (*circuit).loss_energy;
    fprintf(output,
        "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,"
        "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,"
        "%lld,%lld,%.12f,%.12f,%.12f,%.12g,%.9f\n",
        current_time, (*circuit).vc, (*circuit).i, i_ref, ibat, tracking_error, Vg * ibat,
        (*circuit).m, (*controller).mt, (*controller).mt_requested, (*controller).i_sense,
        (*controller).ibat_sense, (*controller).vg_sense, (*controller).vc_sense,
        (*controller).error, (*controller).mt_min, (*controller).mt_max,
        (*stats).sqrt_limit_count, (*stats).duty_limit_count,
        energy, (*circuit).input_energy, (*circuit).loss_energy, energy_residual,
        (*controller).ibat_sense_raw);
}

// Runs the circuit loop, including sensor delays and controller updates.
static int run_simulation(FILE* output, Circuit* circuit, Controller* controller,
    Statistics* stats) {
    double timestep = (*controller).period / controller_steps;
    long long modulation_steps = (long long)controller_steps * modulation_controller_cycles;
    // These arrays hold old samples so the controller sees a sensor delay.
    double delay_vc[DELAY_AMOUNT], delay_i[DELAY_AMOUNT], delay_i_ref[DELAY_AMOUNT];
    double duty_history[DELAY_AMOUNT] = { 0 };
    int next_delay = 0;
    for (int j = 0; j < DELAY_AMOUNT; j++) {
        delay_vc[j] = vc_initial;
        delay_i[j] = i_initial;
        delay_i_ref[j] = 0.0;
    }

    for (long long n = 0; n <= (*stats).steps; n++) {
        double current_time = (double)n * timestep;
        double i_ref = current_amplitude * sin(2.0 * PI * freq_HZ * current_time);

        // Circular buffers return readings from DELAY_AMOUNT circuit steps ago.
        double delayed_vc = delay_vc[next_delay];
        double delayed_i = delay_i[next_delay];
        double delayed_i_ref = delay_i_ref[next_delay];
        delay_vc[next_delay] = (*circuit).vc;
        delay_i[next_delay] = (*circuit).i;
        delay_i_ref[next_delay] = i_ref;
        next_delay = (next_delay + 1) % DELAY_AMOUNT;

        // Update the PI controller and duty cycle at their own slower rates.
        if (n % controller_steps == 0
            && !update_controller(circuit, controller, delayed_i, delayed_i_ref, current_time)) {
            return 0;
        }
        if (n % modulation_steps == 0
            && !update_modulation(circuit, controller, stats, delayed_vc, current_time,
                duty_history, n)) {
            return 0;
        }

        // Save the duty actually applied to interval [n, n+1] after estimating the current state.
        duty_history[n % DELAY_AMOUNT] = (*circuit).m;

        // Log the current state, then advance the circuit by one small step.
        record_results(output, circuit, controller, stats, n, current_time, i_ref);
        if (n == (*stats).steps) break;
        if (!step_circuit(circuit, timestep, Vg)) {
            fprintf(stderr, "Non-finite circuit state after %.9f s.\n", current_time);
            return 0;
        }
    }
    return 1;
}

// Prints tracking accuracy, current limits, and energy statistics.
static void print_summary(const Circuit* circuit, const Controller* controller,
    const Statistics* stats) {
    double timestep = (*controller).period / controller_steps;
    double final_energy = stored_energy(circuit);
    double energy_residual = final_energy - (*stats).initial_energy
        - (*circuit).input_energy + (*circuit).loss_energy;
    double samples = (double)(*stats).metric_count;
    double updates = (double)(*stats).modulation_count;
    printf("Simulation complete. Data saved to %s\n", output_name);
    printf("Model: ideal averaged converter; bidirectional current allowed\n");
    printf("Integrator: RK4, %.0f Hz; controller: %.0f Hz; duty updates: %.0f Hz\n",
        1.0 / timestep, controller_update_Hz, controller_update_Hz / modulation_controller_cycles);
    printf("Bandwidth setting: %.1f Hz; Kp: %.9g; Ki: %.9g\n",
        controller_bandwidth_Hz, (*controller).kp, (*controller).ki);
    printf("Sensor noise: %s; seed: %u\n", noise_enabled ? "on" : "off", (unsigned int)seed);
    printf("Feedback filter: undamped LC model, %.0f Hz updates; natural frequency %.6f Hz\n",
        controller_update_Hz, 1.0 / (2.0 * PI * sqrt(L * C)));
    printf("Current limiter compensates sensor delay using applied-duty history\n");
    printf("Tracking statistics use actual branch current m*I, before feedback filtering\n");
    printf("Statistics interval: %.6f to %.6f s\n",
        (double)(*stats).metric_start * timestep, (double)(*stats).steps * timestep);
    printf("RMS tracking error: %.9f A\n", sqrt((*stats).error_squared_sum / samples));
    printf("Mean tracking error: %.9f A\n", (*stats).error_sum / samples);
    printf("Maximum absolute tracking error: %.9f A\n", (*stats).max_error);
    printf("Mean battery current: %.9f A\n", (*stats).ibat_sum / samples);
    printf("RMS inductor current: %.6f A\n", sqrt((*stats).i_squared_sum / samples));
    printf("Peak absolute inductor current (whole run): %.6f A\n", (*stats).peak_i);
    printf("Duty range (whole run): %.9f to %.9f\n", (*stats).min_m, (*stats).max_m);
    printf("Square-root limit: %lld / %lld updates (%.6f%%)\n",
        (*stats).sqrt_limit_count, (*stats).modulation_count,
        100.0 * (double)(*stats).sqrt_limit_count / updates);
    printf("Duty clipping: %lld / %lld updates (%.6f%%)\n",
        (*stats).duty_limit_count, (*stats).modulation_count,
        100.0 * (double)(*stats).duty_limit_count / updates);
    printf("Current-limited modulation updates: %lld\n", (*stats).current_limit_count);
    printf("Actual current-limit violations: %lld\n", (*stats).current_violation_count);
    printf("Final capacitor voltage: %.9f V\n", (*circuit).vc);
    printf("Stored-energy change: %.9f J\n", final_energy - (*stats).initial_energy);
    printf("Energy from source (signed): %.9f J\n", (*circuit).input_energy);
    printf("Resistive energy loss: %.9f J\n", (*circuit).loss_energy);
    printf("Numerical energy residual: %.12g J\n", energy_residual);
}

// Runs the Python plotting script if plotting is enabled and available.
static int open_plots(void) {
    if (!plot_enabled) return 0;
    FILE* script = fopen("plot_circuit.py", "r");
    if (script == NULL) {
        printf("Plots skipped: plot_circuit.py was not found in the working folder.\n");
        return 0;
    }
    fclose(script);
    printf("Opening plots...\n");
    fflush(stdout);
    if (system("python plot_circuit.py") != 0) {
        fprintf(stderr, "CSV saved successfully, but plotting failed. "
            "Run python plot_circuit.py to inspect the error.\n");
        return 1;
    }
    return 0;
}

// Sets up the simulation, saves its results, and opens plots if requested.
int main(void) {
    Circuit circuit = { 0 };
    Controller controller = { 0 };
    Statistics stats = { 0 };
    if (!initialize_simulation(&circuit, &controller, &stats)) return 1;

    FILE* output = open_output();
    if (output == NULL) return 1;
    int success = run_simulation(output, &circuit, &controller, &stats);
    if (ferror(output)) {
        fprintf(stderr, "Error writing CSV output.\n");
        success = 0;
    }
    if (fclose(output) != 0) {
        perror("Error closing CSV output");
        success = 0;
    }
    if (!success) {
        fprintf(stderr, "Simulation stopped; the CSV may contain partial data.\n");
        return 1;
    }

    print_summary(&circuit, &controller, &stats);
    return open_plots();
}
