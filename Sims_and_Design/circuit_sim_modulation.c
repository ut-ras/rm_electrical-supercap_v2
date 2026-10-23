#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#define PI 3.14159265358979323846
#define DELAY_AMOUNT 30 // for the delays

uint32_t random_state[3];
int have_extra_noise[3] = { 0, 0, 0 };
double extra_noise[3];

//Return a Gaussian sample with apprx 0 mean and std dev of 1
double generate_gaussian(int sensor) {
    if (have_extra_noise[sensor]) { // Reuse the second Gaussian sample from prev calc
        have_extra_noise[sensor] = 0;
        return extra_noise[sensor];
    }

    double u[2]; // Create two values between 0 and 1 if a sample not available
    //Use a XORshift (GPT idea)
    for (int j = 0; j < 2; j++) {
        uint32_t value = random_state[sensor];
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        random_state[sensor] = value;
        u[j] = ((double)value + 0.5) / 4294967296.0;
    }

    //Box-Muller transforms it into two normal samples
    //Return the cosine sample and save sine for next time
    double radius = sqrt(-2.0 * log(u[0]));
    double angle = 2.0 * PI * u[1];
    extra_noise[sensor] = radius * sin(angle);
    have_extra_noise[sensor] = 1;
    return radius * cos(angle);
}

//Read a number from a command-line argument 
//STARTING HERE NOT LOWKEY THAT IMPORTANT FOR CODE FUNCTIONALITY IN TERMS OF SIMULATOR
int read_positive_number(const char* text, double* number) {
    char* end;
    errno = 0;
    *number = strtod(text, &end);
    return errno == 0 && end != text && *end == '\0'
        && isfinite(*number) && *number > 0.0;
}

int read_unsigned_number(const char* text, uint32_t* number) {
    char* end;
    if (*text == '-') return 0;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || end == text || *end != '\0' || value > UINT32_MAX) return 0;
    *number = (uint32_t)value;
    return 1;
}

int main(int argc, char* argv[]) {

    //Circuit parameters and sstuff
    const double C = 440.0;
    const double R = 0.0095;
    const double L = 28.3e-6;
    const double Vg = 24.0;
    const double vc_initial = 2.5;
    const double i_initial = 0.0;
    const double freq_HZ = 100.0;
    const double current_amplitude = 2.5;
    const double i_limit = 30.0;
    const double current_margin = 1.0;
    const double controller_update_Hz = 12000.0;
    const int modulation_controller_cycles = 2;

    //Can be overriden by command-line arguments when calling .exe file
    double controller_bandwidth_Hz = 300.0;
    double total_time = 5.0 * R * C;
    int controller_steps = 10;
    //CHANGE TO A RANDOM SEED WHEN NOT COMPARING SAME NOISE
    uint32_t seed = 1; // Use a fixed seed to compare using same noise for now
    int noise_enabled = 1; // Make 0 if don't want noise
    int plot_enabled = 1; // Make 0 if don't want plots
    const char* output_name = "circuit_data.csv";

    //Function to read command-line arguments, not that importan tto understand
    for (int a = 1; a < argc; a++) {
        if (strcmp(argv[a], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("  --bandwidth HZ   Controller tuning (default 300)\n");
            printf("  --duration SEC   Simulation duration (default 20.9)\n");
            printf("  --substeps N     Circuit steps per controller period (default 10)\n");
            printf("  --seed N         Repeatable noise seed (default 1)\n");
            printf("  --no-noise       Disable sensor noise\n");
            printf("  --output PATH    CSV filename (default circuit_data.csv)\n");
            printf("  --plot           Open plots (enabled by default)\n");
            printf("  --no-plot        Skip plotting\n");
            printf("  --help           Show these options\n");
            return 0;
        }
        if (strcmp(argv[a], "--no-noise") == 0) {
            noise_enabled = 0;
        }
        else if (strcmp(argv[a], "--plot") == 0) {
            plot_enabled = 1;
        }
        else if (strcmp(argv[a], "--no-plot") == 0) {
            plot_enabled = 0;
        }
        else if (a + 1 >= argc) {
            fprintf(stderr, "Missing value or unknown option: %s\n", argv[a]);
            return 1;
        }
        else if (strcmp(argv[a], "--bandwidth") == 0) {
            if (!read_positive_number(argv[++a], &controller_bandwidth_Hz)) return 1;
        }
        else if (strcmp(argv[a], "--duration") == 0) {
            if (!read_positive_number(argv[++a], &total_time)) return 1;
        }
        else if (strcmp(argv[a], "--substeps") == 0) {
            uint32_t value;
            if (!read_unsigned_number(argv[++a], &value) || value < 1 || value > 10000)
                return 1;
            controller_steps = (int)value;
        }
        else if (strcmp(argv[a], "--seed") == 0) {
            if (!read_unsigned_number(argv[++a], &seed)) return 1;
        }
        else if (strcmp(argv[a], "--output") == 0) {
            output_name = argv[++a];
            if (*output_name == '\0') return 1;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[a]);
            return 1;
        }
    }

    if (!(C > 0.0 && R >= 0.0 && L > 0.0 && Vg > 0.0
        && controller_update_Hz > 0.0 && modulation_controller_cycles > 0)) {
        fprintf(stderr, "Invalid circuit parameters or update rates.\n");
        return 1;
    }
    if (plot_enabled && strcmp(output_name, "circuit_data.csv") != 0) {
        fprintf(stderr, "Use --no-plot with a custom --output filename.\n");
        return 1;
    }

    //Create update rates for controller, timestep, and modulation
    const double controller_period = 1.0 / controller_update_Hz;
    const double timestep = controller_period / controller_steps;
    const long long modulation_steps = (long long)controller_steps
        * modulation_controller_cycles;
    double step_count = total_time / timestep;
    if (!isfinite(step_count) || step_count < 1.0 || step_count > 1e12) {
        fprintf(stderr, "Duration must span 1 to 1e12 circuit steps.\n");
        return 1;
    }
    const long long steps = llround(step_count);

    //Calc PI gains from the transformed-current model
    // 300 HZ setting improved the tests, not full-range stability
    const double tau_i = 1.0 / (2.0 * PI * controller_bandwidth_Hz);
    const double kp_pi = L / (Vg * tau_i);
    const double ki_pi = R / (Vg * tau_i);
    if (!isfinite(kp_pi) || !isfinite(ki_pi)) {
        fprintf(stderr, "Controller gains are not finite.\n");
        return 1;
    }

    //Create separate nonzero states for each sensor
    // Hex tags distinguish the three startin gstates
    uint32_t sensor_tags[3] = { UINT32_C(0xa341316c), UINT32_C(0xc8013ea4),
                               UINT32_C(0xad90777d) };
    for (int sensor = 0; sensor < 3; sensor++) {
        random_state[sensor] = seed ^ sensor_tags[sensor];
        if (random_state[sensor] == 0) random_state[sensor] = UINT32_C(0x6d2b79f5);
    }

    //Initalize the true physical state
    double vc_n = vc_initial;
    double i_n = i_initial;
    double m_n = fmax(0.0, fmin(1.0, vc_n / Vg));
    double Vgsense = Vg, Vcsense = vc_n, i_nsense = i_n;
    double ibat_nsense = m_n * i_nsense;
    double error_sense = 0.0;
    //MT = m^2 - (Vc/Vg)*m is the modulation command
    //pi_integral stores the integral contribution in MT units, not a physical current
    double mt_n = m_n * m_n - (Vcsense / Vgsense) * m_n;
    double mt_requested = mt_n;
    double pi_integral = mt_n;
    double error_prev = 0.0;
    double mt_min = 0.0, mt_max = 0.0;

    //Tracking statistics uses the final second (or full run if shorter than one second)
    long long one_second_steps = llround(1.0 / timestep);
    long long metric_steps = steps < one_second_steps ? steps : one_second_steps;
    long long metric_start = steps - metric_steps;
    //Counters just to help see how many "violations" of what limits we have set happen
    long long metric_count = 0, modulation_count = 0;
    long long sqrt_limit_count = 0, duty_limit_count = 0;
    long long current_limit_count = 0;
    long long current_violation_count = 0;
    double error_squared_sum = 0.0, error_sum = 0.0, ibat_sum = 0.0;
    double i_squared_sum = 0.0, max_error = 0.0;
    //peak_i stores the largest magnitude (positive and negative current both included)
    double peak_i = fabs(i_n), min_m = m_n, max_m = m_n;
    //Stored energy is E = .5 * C * Vc^2 + .5 * L *i^2 (in joules)
    //Energy not needed for PI controller and current limiter, just to help check if stuff makes sense
    //in a physical world 
    double initial_energy = 0.5 * C * vc_n * vc_n + 0.5 * L * i_n * i_n;
    double input_energy = 0.0, loss_energy = 0.0;

    FILE* output = fopen(output_name, "w");
    if (output == NULL) {
        perror("Cannot open CSV output");
        return 1;
    }
    setvbuf(output, NULL, _IOFBF, 1024 * 1024);
    fprintf(output, "Time,Vc,I,Iref,Ibat,Error,Power,M,MT,MT_requested,"
        "Isense,Ibatsense,Vgsense,Vcsense,Error_sense,MT_min,MT_max,"
        "Sqrt_limits,Duty_limits,Energy_J,Input_energy_J,"
        "Loss_energy_J,Energy_residual_J\n");

    //Use success in case error happens where success is set to 0
    int success = 1;
    double delay_line_Vc[DELAY_AMOUNT]; // for the delay Vc
    double delay_line_I_n[DELAY_AMOUNT]; // for the delay I_n
    double delay_line_i_ref[DELAY_AMOUNT]; // for the delay i_ref

    for (long long n = 0; n <= steps; n++) {

        // in controller delay
        double current_Vc_delay = vc_n;
        double current_i_n_delay = i_n;

        delay_line_Vc[DELAY_AMOUNT - 1] = current_Vc_delay;
        for (int i = DELAY_AMOUNT - 1; i > 0; i--) { // Shift the delay line to the right
            delay_line_Vc[i] = delay_line_Vc[i - 1];
        }
        double delayed_vc_n = delay_line_Vc[0];

        delay_line_i_n[DELAY_AMOUNT - 1] = current_i_n_delay;
        for (int i = DELAY_AMOUNT - 1; i > 0; i--) { // Shift the delay line to the right
            delay_line_i_n[i] = delay_line_i_n[i - 1];
        }
        double delayed_i_n = delay_line_i_n[0];

        double current_time = (double)n * timestep;
        double current_i_ref = current_amplitude * sin(2.0 * PI * freq_HZ * current_time);

        delay_line_i_ref[DELAY_AMOUNT - 1] = current_i_ref;
        for (int i = DELAY_AMOUNT - 1; i > 0; i--) { // Shift the delay line to the right
            delay_line_i_ref[i] = delay_line_i_ref[i - 1];
        }
        double delayed_i_ref = delay_line_i_ref[0];

        if (n % controller_steps == 0) {
            double vg_noise = 0.0, i_noise = 0.0; i_ref_noise = 0.0;

            //Standard-normal sample has std dev 1 so we scale it to the sensor noise in V or A

            if (noise_enabled) {
                vg_noise = 0.0903 * generate_gaussian(0);
                i_noise = 0.141 * generate_gaussian(1);
                i_ref_noise = 0.0855 * generate_gaussian(1);
            }
            Vgsense = Vg + vg_noise;
            i_nsense = delayed_i_n + i_noise;
            i_ref_sense = delayed_i_ref + i_ref_noise;

            //Controller divides by Vgsense so just check it before
            if (!isfinite(Vgsense) || Vgsense <= 0.0) {
                fprintf(stderr, "Invalid sensed source voltage at %.9f s.\n", current_time);
                success = 0;
                break;
            }


            ibat_nsense = m_n * i_nsense;
            error_sense = i_ref_sense - ibat_nsense;
            delayed_time = current_time - controller_period * DELAY_AMOUNT;
            //Use feedforward with derivatives, omits the term L*i_n*dm/dt
            double i_ref_derivative = current_amplitude * 2.0 * PI * freq_HZ
                * cos(2.0 * PI * freq_HZ * delayed_time); // FYI, this line may to be updated as it doesn't account for any of the noise and only does for the time with delayed_time ***
            double feedforward = (L * i_ref_derivative + R * i_ref_sense) / Vgsense;
            //Integrate the error using average of this sample and previous
            //Use controller period here not the timestep
            pi_integral += 0.5 * controller_period * ki_pi * (error_sense + error_prev);
            mt_requested = kp_pi * error_sense + pi_integral + feedforward;
            //Save for next integral update
            error_prev = error_sense;
            if (!isfinite(mt_requested)) {
                fprintf(stderr, "Non-finite controller command at %.9f s.\n", current_time);
                success = 0;
                break;
            }
        }

        if (n % modulation_steps == 0) {
            double vc_noise = 0.0;
            if (noise_enabled) vc_noise = 0.0103 * generate_gaussian(2);
            
            Vcsense = delayed_vc_n + vc_noise;
            // Let kt = Vc(2*Vg), completing square gives MT + kt^2 = (m-kt)^2
            double kt_n = Vcsense / (2.0 * Vgsense);
            //Check domain of the chosen model (NOT the 3V rating)
            if (!isfinite(kt_n) || kt_n < 0.0 || kt_n >= 1.0) {
                fprintf(stderr, "Sensed Vc/Vg is outside the upper-root model range "
                    "at %.9f s.\n", current_time);
                success = 0;
                break;
            }

            //MT bounds
            mt_min = -kt_n * kt_n;
            mt_max = 1.0 - 2.0 * kt_n;
            double sqrt_term = mt_requested + kt_n * kt_n;
            double m_n1 = kt_n + sqrt(fmax(0.0, sqrt_term));
            if (sqrt_term < 0.0) sqrt_limit_count++;
            if (m_n1 < 0.0 || m_n1 > 1.0) duty_limit_count++;
            modulation_count++;
            // Predict across the full interval before modulation can change again.
            //Controller needs to predict until next time m_n is changed
            double h = controller_period * modulation_controller_cycles;
            //Trapezoid integration of both physical equations (beta is in terms of ohms)
            double beta = R / 2.0 + h / (4.0 * C);
            //Prediction target is +/- 29A, violation threshold is still +/- 30A
            //Use 29 instead of 30 to help with overshooting that will happen
            //Egan wanted max of 30 that's why this part exists
            double i_control_limit = i_limit - current_margin;

            // Duties that put the predicted next current at the lower and upper limits.
            double m_min = (
                Vcsense
                + (L / h) * (-i_control_limit - i_nsense)
                + beta * (-i_control_limit + i_nsense)
                ) / Vgsense;

            double m_max = (
                Vcsense
                + (L / h) * (i_control_limit - i_nsense)
                + beta * (i_control_limit + i_nsense)
                ) / Vgsense;

            // Intersect the current-dependent bounds with the available duty range.
            m_min = fmax(0.0, m_min);
            m_max = fmin(1.0, m_max);

            if (m_min > m_max) {
                fprintf(
                    stderr,
                    "No feasible current-limited duty at %.9f s.\n",
                    current_time
                );
                success = 0;
                break;
            }

            //Apply ordinary duty range, then check if current constraint needs more change
            double proposed_m = fmax(0.0, fmin(1.0, m_n1));

            if (proposed_m < m_min || proposed_m > m_max) {
                current_limit_count++;
            }

            //Choose closest duty inside allowed interval, changes circuit INPUT
            //i_n is never assigned clipped value
            m_n = fmax(m_min, fmin(m_max, proposed_m));

            // Reconcile the transformed command and PI integral with the final duty.
            mt_n = m_n * m_n - 2.0 * kt_n * m_n;
            pi_integral += mt_n - mt_requested;
        }

        //Calculate actual current, error and power
        //Use true simulated quantities for performance, not with noise
        //ibat_n is the converter bus branch, external load total battery current becomes
        // load current + ibat_n
        double ibat_n = m_n * i_n;

        // for the low pass LC filter
        // double Fcutoff = 1/(2* PI * sqrt(L * C));
        // double ratio = freq_HZ / Fcutoff;
        // double Z0 = sqrt(L/C);
        // ibat_n = ibat_n * (1.0 / sqrt(
        //     ((1.0-ratio*ratio)*(1.0-ratio*ratio)) +
        //     (((1.0/R)* Z0 * ratio)*((1.0/R)* Z0 * ratio) ) 
        // ));

        double tracking_error = i_ref - ibat_n;
        //Positive is charging, negative is returning energy
        double battery_power = Vg * ibat_n;
        double energy = 0.5 * C * vc_n * vc_n + 0.5 * L * i_n * i_n;
        //Energy identity is input_energy - loss_energy
        double energy_residual = energy - initial_energy - input_energy + loss_energy;
        peak_i = fmax(peak_i, fabs(i_n));
        if (fabs(i_n) > i_limit + 1e-6) {
            current_violation_count++;
        }
        min_m = fmin(min_m, m_n);
        max_m = fmax(max_m, m_n);
        if (n >= metric_start && n < steps) {
            metric_count++;
            error_squared_sum += tracking_error * tracking_error;
            error_sum += tracking_error;
            ibat_sum += ibat_n;
            i_squared_sum += i_n * i_n;
            max_error = fmax(max_error, fabs(tracking_error));
        }
        if (n % controller_steps == 0 || n == steps) {
            fprintf(output,
                "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,"
                "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,"
                "%lld,%lld,%.12f,%.12f,%.12f,%.12g\n",
                current_time, vc_n, i_n, i_ref, ibat_n, tracking_error, battery_power,
                m_n, mt_n, mt_requested, i_nsense, ibat_nsense, Vgsense, Vcsense,
                error_sense, mt_min, mt_max, sqrt_limit_count, duty_limit_count,
                energy, input_energy, loss_energy, energy_residual);
        }
        if (n == steps) break;

        // RK4: keep m_n fixed while calculating the four sets of slopes.
        //GPT thought RK4 was better than Euler and it did help with error so
        //Stage 1: evaluate dVc/dt and di/dt at starting state
        double k1_vc = i_n / C;
        double k1_i = (m_n * Vg - R * i_n - vc_n) / L;
        //Stage 2: estimate midpoint using slopes from stage1 and evaluate new slopes here
        double vc2 = vc_n + (0.5 * timestep) * k1_vc;
        double i2 = i_n + (0.5 * timestep) * k1_i;
        double k2_vc = i2 / C;
        double k2_i = (m_n * Vg - R * i2 - vc2) / L;
        //Stage 3: New midpoint estimate from original state using stage 2 slopes
        double vc3 = vc_n + (0.5 * timestep) * k2_vc;
        double i3 = i_n + (0.5 * timestep) * k2_i;
        double k3_vc = i3 / C;
        double k3_i = (m_n * Vg - R * i3 - vc3) / L;
        //Stage 4: Estimate endpoint using stage3 slopes and get endpoint slopes
        double vc4 = vc_n + timestep * k3_vc;
        double i4 = i_n + timestep * k3_i;
        double k4_vc = i4 / C;
        double k4_i = (m_n * Vg - R * i4 - vc4) / L;

        // Integrate input power Vg*m*i and resistor power R*i^2 with RK4 stage currents
        input_energy += timestep * Vg * m_n * (i_n + 2.0 * i2 + 2.0 * i3 + i4) / 6.0;
        loss_energy += timestep * R
            * (i_n * i_n + 2.0 * i2 * i2 + 2.0 * i3 * i3 + i4 * i4) / 6.0;
        //Apply the Rk4 weighted average to both states
        vc_n += timestep * (k1_vc + 2.0 * k2_vc + 2.0 * k3_vc + k4_vc) / 6.0;
        i_n += timestep * (k1_i + 2.0 * k2_i + 2.0 * k3_i + k4_i) / 6.0;
        if (!isfinite(vc_n) || !isfinite(i_n)
            || !isfinite(input_energy) || !isfinite(loss_energy)) {
            fprintf(stderr, "Non-finite circuit state after %.9f s.\n", current_time);
            success = 0;
            break;
        }
    }

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

    double final_energy = 0.5 * C * vc_n * vc_n + 0.5 * L * i_n * i_n;
    double energy_residual = final_energy - initial_energy - input_energy + loss_energy;
    double samples = (double)metric_count;
    double updates = (double)modulation_count;
    printf("Simulation complete. Data saved to %s\n", output_name);
    printf("Model: ideal averaged converter; bidirectional current allowed\n");
    printf("Integrator: RK4, %.0f Hz; controller: %.0f Hz; duty updates: %.0f Hz\n",
        1.0 / timestep, controller_update_Hz, controller_update_Hz / modulation_controller_cycles);
    printf("Bandwidth setting: %.1f Hz; Kp: %.9g; Ki: %.9g\n", controller_bandwidth_Hz, kp_pi, ki_pi);
    printf("Sensor noise: %s; seed: %u\n", noise_enabled ? "on" : "off", (unsigned int)seed);
    printf("Statistics interval: %.6f to %.6f s\n", (double)metric_start * timestep, (double)steps * timestep);
    printf("RMS tracking error: %.9f A\n", sqrt(error_squared_sum / samples));
    printf("Mean tracking error: %.9f A\n", error_sum / samples);
    printf("Maximum absolute tracking error: %.9f A\n", max_error);
    printf("Mean battery current: %.9f A\n", ibat_sum / samples);
    printf("RMS inductor current: %.6f A\n", sqrt(i_squared_sum / samples));
    printf("Peak absolute inductor current (whole run): %.6f A\n", peak_i);
    printf("Duty range (whole run): %.9f to %.9f\n", min_m, max_m);
    printf("Square-root limit: %lld / %lld updates (%.6f%%)\n",
        sqrt_limit_count, modulation_count, 100.0 * (double)sqrt_limit_count / updates);
    printf("Duty clipping: %lld / %lld updates (%.6f%%)\n",
        duty_limit_count, modulation_count, 100.0 * (double)duty_limit_count / updates);
    printf(
        "Current-limited modulation updates: %lld\n",
        current_limit_count
    );

    printf(
        "Actual current-limit violations: %lld\n",
        current_violation_count
    );
    printf("Final capacitor voltage: %.9f V\n", vc_n);
    printf("Stored-energy change: %.9f J\n", final_energy - initial_energy);
    printf("Energy from source (signed): %.9f J\n", input_energy);
    printf("Resistive energy loss: %.9f J\n", loss_energy);
    printf("Numerical energy residual: %.12g J\n", energy_residual);

    if (plot_enabled) {
        printf("Opening plots...\n");
        fflush(stdout);
        if (system("py -3.15 plot_circuit.py") != 0) {
            fprintf(stderr, "CSV saved successfully, but plotting failed. "
                "Run python plot_circuit.py to inspect the error.\n");
            return 1;
        }
    }
    return 0;
}
