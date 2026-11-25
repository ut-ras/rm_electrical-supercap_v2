#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <math.h>
#include <stdint.h>

/* ---------------- Simulation settings ---------------- */
#define PI                  3.14159265358979323846
#define TWO_PI              (2.0 * PI)
#define DELAY_AMOUNT        30        /* Sensor delay in circuit timesteps */

#define CAPACITANCE         440.0     /* F */
#define RESISTANCE          0.0095    /* ohms */
#define INDUCTANCE          28.3e-6   /* H */
#define VG                  24.0      /* Source voltage (V) */
#define VC_INITIAL          2.5
#define I_INITIAL           0.0
#define FREQ_HZ             100.0
#define CURRENT_AMPLITUDE   2.5
#define I_LIMIT             30.0
#define CURRENT_MARGIN      1.0
#define CONTROLLER_UPDATE_HZ        12000.0
#define CONTROLLER_BANDWIDTH_HZ     300.0
#define CONTROLLER_STEPS            10   /* Circuit steps per controller update */
#define MODULATION_CONTROLLER_CYCLES 2
#define TOTAL_TIME          20.9      /* s */
#define SEED                1u
#define NOISE_ENABLED       1
#define OUTPUT_NAME         "circuit_data.csv"

/* ---------------- Sensor noise ---------------- */
#define NUM_SENSORS         3
#define SENSOR_VG           0
#define SENSOR_I            1         /* Shared by I and Iref sensors */
#define SENSOR_VC           2
#define SIGMA_VG            0.0903
#define SIGMA_I             0.141
#define SIGMA_IREF          0.0855
#define SIGMA_VC            0.0103
#define SENSOR_TAG_0        UINT32_C(0xa341316c)
#define SENSOR_TAG_1        UINT32_C(0xc8013ea4)
#define SENSOR_TAG_2        UINT32_C(0xad90777d)
#define FALLBACK_RNG_STATE  UINT32_C(0x6d2b79f5)
#define XORSHIFT_A          13
#define XORSHIFT_B          17
#define XORSHIFT_C          5
#define UINT32_RANGE        4294967296.0

/* ---------------- Derived quantities ---------------- */
#define CONTROLLER_PERIOD   (1.0 / CONTROLLER_UPDATE_HZ)
#define TIMESTEP            (CONTROLLER_PERIOD / CONTROLLER_STEPS)
#define MODULATION_STEPS    (CONTROLLER_STEPS * MODULATION_CONTROLLER_CYCLES)
#define TOTAL_STEPS         llround(TOTAL_TIME / TIMESTEP)

#define TAU_I               (1.0 / (TWO_PI * CONTROLLER_BANDWIDTH_HZ))
#define KP                  (INDUCTANCE / (VG * TAU_I))
#define KI                  (RESISTANCE / (VG * TAU_I))

/* Discrete form of H(s) = 1 / (L*C*s^2 + 1) (bilinear transform) */
#define FILTER_T2           (CONTROLLER_PERIOD * CONTROLLER_PERIOD)
#define FILTER_DENOM        (4.0 * INDUCTANCE * CAPACITANCE + FILTER_T2)
#define FILTER_INPUT_GAIN   (FILTER_T2 / FILTER_DENOM)
#define FILTER_OUTPUT_GAIN  ((8.0 * INDUCTANCE * CAPACITANCE - 2.0 * FILTER_T2) / FILTER_DENOM)

/* Current-limit duty bounds */
#define LIMIT_H             (CONTROLLER_PERIOD * MODULATION_CONTROLLER_CYCLES)
#define LIMIT_BETA          (RESISTANCE / 2.0 + LIMIT_H / (4.0 * CAPACITANCE))
#define CONTROL_LIMIT       (I_LIMIT - CURRENT_MARGIN)

/* ---------------- State ---------------- */
typedef struct { double vc, i, m; } Circuit;

typedef struct {
    double vg_sense, vc_sense, i_sense;
    double error, previous_error;
    double mt_requested, integral;
    double in1, in2, out1, out2;  /* Battery-current filter history */
} Controller;

static uint32_t random_state[NUM_SENSORS];
static int have_extra[NUM_SENSORS];
static double extra_noise[NUM_SENSORS];

static double gaussian(int sensor) {
    if (have_extra[sensor]) {
        have_extra[sensor] = 0;
        return extra_noise[sensor];
    }
    double u[2];
    for (int j = 0; j < 2; j++) {
        uint32_t v = random_state[sensor];
        v ^= v << XORSHIFT_A;
        v ^= v >> XORSHIFT_B;
        v ^= v << XORSHIFT_C;
        random_state[sensor] = v;
        u[j] = ((double)v + 0.5) / UINT32_RANGE;
    }
    double radius = sqrt(-2.0 * log(u[0]));
    double angle = TWO_PI * u[1];
    extra_noise[sensor] = radius * sin(angle);
    have_extra[sensor] = 1;
    return radius * cos(angle);
}

static double clamp(double x, double lo, double hi) {
    return fmax(lo, fmin(hi, x));
}

/* One RK4 step of: dVc/dt = I/C, dI/dt = (m*Vg - R*I - Vc)/L */
static void step_circuit(Circuit* c, double dt, double vg) {
    double vc = c->vc, i = c->i, m = c->m;
    double k1v = i / CAPACITANCE;
    double k1i = (m * vg - RESISTANCE * i - vc) / INDUCTANCE;
    double vc2 = vc + 0.5 * dt * k1v, i2 = i + 0.5 * dt * k1i;
    double k2v = i2 / CAPACITANCE;
    double k2i = (m * vg - RESISTANCE * i2 - vc2) / INDUCTANCE;
    double vc3 = vc + 0.5 * dt * k2v, i3 = i + 0.5 * dt * k2i;
    double k3v = i3 / CAPACITANCE;
    double k3i = (m * vg - RESISTANCE * i3 - vc3) / INDUCTANCE;
    double vc4 = vc + dt * k3v, i4 = i + dt * k3i;
    double k4v = i4 / CAPACITANCE;
    double k4i = (m * vg - RESISTANCE * i4 - vc4) / INDUCTANCE;
    c->vc += dt * (k1v + 2.0 * k2v + 2.0 * k3v + k4v) / 6.0;
    c->i += dt * (k1i + 2.0 * k2i + 2.0 * k3i + k4i) / 6.0;
}

static double filter_battery_current(Controller* k, double input) {
    double out = FILTER_INPUT_GAIN * (input + 2.0 * k->in1 + k->in2)
        + FILTER_OUTPUT_GAIN * k->out1 - k->out2;
    k->in2 = k->in1;   k->in1 = input;
    k->out2 = k->out1; k->out1 = out;
    return out;
}

static int update_controller(const Circuit* c, Controller* k,
    double delayed_i, double delayed_i_ref, double t) {
    double vg_noise = 0, i_noise = 0, iref_noise = 0;
    if (NOISE_ENABLED) {
        vg_noise = SIGMA_VG * gaussian(SENSOR_VG);
        i_noise = SIGMA_I * gaussian(SENSOR_I);
        iref_noise = SIGMA_IREF * gaussian(SENSOR_I);
    }
    k->vg_sense = VG + vg_noise;
    k->i_sense = delayed_i + i_noise;
    double i_ref_sense = delayed_i_ref + iref_noise;
    if (k->vg_sense <= 0.0) {
        fprintf(stderr, "Invalid sensed source voltage at %.9f s.\n", t);
        return 0;
    }

    double ibat = filter_battery_current(k, c->m * k->i_sense);
    k->error = i_ref_sense - ibat;

    double delayed_time = t - TIMESTEP * DELAY_AMOUNT;
    double i_ref_deriv = CURRENT_AMPLITUDE * TWO_PI * FREQ_HZ
        * cos(TWO_PI * FREQ_HZ * delayed_time);
    double feedforward = (INDUCTANCE * i_ref_deriv + RESISTANCE * i_ref_sense) / k->vg_sense;

    k->integral += 0.5 * CONTROLLER_PERIOD * KI * (k->error + k->previous_error);
    k->mt_requested = KP * k->error + k->integral + feedforward;
    k->previous_error = k->error;
    return 1;
}

/* Replay duties applied since the delayed sample to estimate the present state. */
static void predict_present_state(const Controller* k, const double duty_history[DELAY_AMOUNT],
    long long n, double* vc, double* i) {
    Circuit p = { k->vc_sense, k->i_sense, 0.0 };
    int count = n < DELAY_AMOUNT ? (int)n : DELAY_AMOUNT;
    int first = n < DELAY_AMOUNT ? 0 : (int)(n % DELAY_AMOUNT);
    for (int j = 0; j < count; j++) {
        p.m = duty_history[(first + j) % DELAY_AMOUNT];
        step_circuit(&p, TIMESTEP, k->vg_sense);
    }
    *vc = p.vc;
    *i = p.i;
}

static int update_modulation(Circuit* c, Controller* k, double delayed_vc, double t,
    const double duty_history[DELAY_AMOUNT], long long n) {
    double vc_noise = NOISE_ENABLED ? SIGMA_VC * gaussian(SENSOR_VC) : 0.0;
    k->vc_sense = delayed_vc + vc_noise;
    double kt = k->vc_sense / (2.0 * k->vg_sense);
    if (kt < 0.0 || kt >= 1.0) {
        fprintf(stderr, "Sensed Vc/Vg is outside the model range at %.9f s.\n", t);
        return 0;
    }

    /* Solve MT = m^2 - 2*kt*m for m (upper root) */
    double proposed_m = kt + sqrt(fmax(0.0, k->mt_requested + kt * kt));

    double pvc, pi;
    predict_present_state(k, duty_history, n, &pvc, &pi);
    double m_min = (pvc + (INDUCTANCE / LIMIT_H) * (-CONTROL_LIMIT - pi)
        + LIMIT_BETA * (-CONTROL_LIMIT + pi)) / k->vg_sense;
    double m_max = (pvc + (INDUCTANCE / LIMIT_H) * (CONTROL_LIMIT - pi)
        + LIMIT_BETA * (CONTROL_LIMIT + pi)) / k->vg_sense;
    m_min = fmax(0.0, m_min);
    m_max = fmin(1.0, m_max);
    if (m_min > m_max) {
        fprintf(stderr, "No feasible current-limited duty at %.9f s.\n", t);
        return 0;
    }
    c->m = clamp(clamp(proposed_m, 0.0, 1.0), m_min, m_max);

    /* Anti-windup: remove the part of the command that limits cut off */
    double mt = c->m * c->m - 2.0 * kt * c->m;
    k->integral += mt - k->mt_requested;
    return 1;
}

int main(void) {
    const uint32_t tags[NUM_SENSORS] = { SENSOR_TAG_0, SENSOR_TAG_1, SENSOR_TAG_2 };
    for (int s = 0; s < NUM_SENSORS; s++) {
        random_state[s] = SEED ^ tags[s];
        if (random_state[s] == 0) random_state[s] = FALLBACK_RNG_STATE;
    }

    Circuit c = { VC_INITIAL, I_INITIAL, 0.0 };
    c.m = clamp(c.vc / VG, 0.0, 1.0);

    double ibat0 = c.m * c.i;
    Controller k = { 0 };
    k.vg_sense = VG;
    k.vc_sense = c.vc;
    k.i_sense = c.i;
    k.in1 = k.in2 = k.out1 = k.out2 = ibat0;
    k.mt_requested = c.m * c.m - (k.vc_sense / k.vg_sense) * c.m;
    k.integral = k.mt_requested;

    FILE* out = fopen(OUTPUT_NAME, "w");
    if (!out) { perror("Cannot open CSV output"); return 1; }
    fprintf(out, "Time,Vc,I,Iref,Ibat,M\n");

    double delay_vc[DELAY_AMOUNT], delay_i[DELAY_AMOUNT], delay_i_ref[DELAY_AMOUNT];
    double duty_history[DELAY_AMOUNT] = { 0 };
    for (int j = 0; j < DELAY_AMOUNT; j++) {
        delay_vc[j] = VC_INITIAL;
        delay_i[j] = I_INITIAL;
        delay_i_ref[j] = 0.0;
    }
    int next_delay = 0;
    const long long steps = TOTAL_STEPS;

    for (long long n = 0; n <= steps; n++) {
        double t = (double)n * TIMESTEP;
        double i_ref = CURRENT_AMPLITUDE * sin(TWO_PI * FREQ_HZ * t);

        double delayed_vc = delay_vc[next_delay];
        double delayed_i = delay_i[next_delay];
        double delayed_i_ref = delay_i_ref[next_delay];
        delay_vc[next_delay] = c.vc;
        delay_i[next_delay] = c.i;
        delay_i_ref[next_delay] = i_ref;
        next_delay = (next_delay + 1) % DELAY_AMOUNT;

        if (n % CONTROLLER_STEPS == 0 && !update_controller(&c, &k, delayed_i, delayed_i_ref, t))
            break;
        if (n % MODULATION_STEPS == 0 && !update_modulation(&c, &k, delayed_vc, t, duty_history, n))
            break;
        duty_history[n % DELAY_AMOUNT] = c.m;

        if (n % CONTROLLER_STEPS == 0 || n == steps)
            fprintf(out, "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
                t, c.vc, c.i, i_ref, c.m * c.i, c.m);

        if (n == steps) break;
        step_circuit(&c, TIMESTEP, VG);
        if (!isfinite(c.vc) || !isfinite(c.i)) {
            fprintf(stderr, "Non-finite circuit state after %.9f s.\n", t);
            fclose(out);
            return 1;
        }
    }

    fclose(out);
    printf("Simulation complete. Data saved to %s\n", OUTPUT_NAME);
    return 0;
}