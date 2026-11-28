#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

/* =====================================================================
   C translation of "Power Stage Simulation - Toy Model.plecs"

   Plant   : averaged converter, V_g -> (duty M) -> L + R -> supercap C
   Control : i_ref = max(I_min, sine);  e = i_ref - i_bat_sensed
             M = min(PI(e), 3/V_g)
   Noise   : battery-current sensor and capacitor-voltage sensor,
             white noise sampled at 1/fsw (held between samples)
   ===================================================================== */

#define PI                  3.14159265358979323846
#define TWO_PI              (2.0 * PI)

/* ---- Parameters from the PLECS InitializationCommands ---- */
#define V_G                 24.0          /* Nominal battery voltage (V) */
#define FSW                 120e3         /* Switching frequency (Hz) */
#define V_C0                0.0           /* Initial supercap voltage (V) */
#define I_L0                0.0           /* Initial inductor current (A) */

#define R_ON                0.35e-3       /* MOSFET on-resistance (ohm) */
#define R_L                 4.3e-3        /* Inductor ESR (ohm) */
#define R_ESR               18e-3         /* Supercap ESR (ohm) */
#define NUM_SUPERCAPS       4.0           /* Supercaps in parallel */
#define R_TOTAL             (2.0 * R_ON + R_L + R_ESR / NUM_SUPERCAPS)

#define L_BLK               28.306e-6     /* Bulk inductance (H) */
#define C_SC                110.0         /* One supercap (F) */
#define C_TOTAL             (NUM_SUPERCAPS * C_SC)

#define PI_BANDWIDTH_SCALE  1e-4          /* tau_i = 1/(2*pi*fsw*1e-4) */
#define TAU_I               (1.0 / (TWO_PI * FSW * PI_BANDWIDTH_SCALE))
#define KP_PI               (L_BLK / TAU_I)   /* Proportional gain (ohm) */
#define KI_PI               (R_TOTAL / TAU_I) /* Integral gain (ohm/s) */

/* ---- Reference, limits and noise (from the schematic blocks) ---- */
#define REF_POWER_W         90.0
#define REF_AMPLITUDE       (REF_POWER_W / V_G)   /* Sine amplitude = 3.75 A */
#define REF_OMEGA           1.0           /* Sine frequency (rad/s) */
#define MAX_VC              3.0           /* Supercap voltage rating (V) */
#define DUTY_MAX            (MAX_VC / V_G)        /* "3/V_g" constant */
#define IMIN_DIVISOR        4.0           /* I_min = -vc^2 / (4*V_g*R) */

/* 1 = add a feedforward duty on top of the PI (0 = original PLECS controller).
   It solves the steady-state plant equation m*V_g = vc + R*(i_ref/m) for m,
   i.e. m^2*V_g - vc*m - R*i_ref = 0 (upper root). I_min is exactly the i_ref
   at which this has a double root, so the square root is always real. */
#define FEEDFORWARD_ENABLED 1

/* 1 = never let the duty drop below vc/(2*V_g), the maximum-power point.
   The I_min limit sits exactly on that point, and below it the converter
   delivers LESS power as the duty falls, so the PI winds the duty down and the
   capacitor voltage collapses. With the PI alone the original PLECS model
   never gets there (results are identical with this on or off), but the
   feedforward can, so this keeps it stable. */
#define MPP_FLOOR_ENABLED   1
#define FF_QUAD_FACTOR      4.0           /* Discriminant: vc^2 + 4*V_g*R*i_ref */
#define FF_ROOT_DIVISOR     2.0           /* m = (vc + sqrt(disc)) / (2*V_g) */

#define NOISE_ENABLED       1
#define SIGMA_BAT           0.0855        /* "bat noise" sigma (A) */
#define SIGMA_CAP           0.0103        /* "cap noise" sigma (V) */

/* ---- Simulation settings ---- */
#define TOTAL_TIME          100.0         /* s (PLECS TimeSpan) */
#define TIMESTEP            (1.0 / FSW)   /* One step per noise sample */
#define TOTAL_STEPS         llround(TOTAL_TIME / TIMESTEP)
#define LOG_INTERVAL        1e-3          /* CSV row spacing (s) */
#define LOG_EVERY           llround(LOG_INTERVAL / TIMESTEP)
#define SEED                0u            /* 0 = new noise every run */
#define SEED_TIME_SHIFT     20
#define OUTPUT_NAME         "plecs_data.csv"

/* ---- Random number generator constants ---- */
#define NUM_SENSORS         2
#define SENSOR_BAT          0
#define SENSOR_CAP          1
#define SENSOR_TAG_0        UINT32_C(0xa341316c)
#define SENSOR_TAG_1        UINT32_C(0xc8013ea4)
#define SPLITMIX_INCREMENT  UINT64_C(0x9e3779b97f4a7c15)
#define SPLITMIX_MULT_1     UINT64_C(0xbf58476d1ce4e5b9)
#define SPLITMIX_MULT_2     UINT64_C(0x94d049bb133111eb)
#define SPLITMIX_SHIFT_1    30
#define SPLITMIX_SHIFT_2    27
#define SPLITMIX_SHIFT_3    31
#define UNIFORM_SHIFT       11
#define TWO_POW_NEG_53      1.1102230246251565e-16
#define PROB_LOW_TAIL       0.02425

typedef struct { double i, vc, x; } State;      /* L current, cap voltage, PI integrator */
typedef struct { double bat, cap; } Noise;      /* Held noise samples */
typedef struct { double m, m_ff, error, i_ref, i_min, i_bat_sense; } Control;

static uint64_t random_state[NUM_SENSORS];

static double uniform_open(int sensor) {
    uint64_t z = (random_state[sensor] += SPLITMIX_INCREMENT);
    z = (z ^ (z >> SPLITMIX_SHIFT_1)) * SPLITMIX_MULT_1;
    z = (z ^ (z >> SPLITMIX_SHIFT_2)) * SPLITMIX_MULT_2;
    z ^= z >> SPLITMIX_SHIFT_3;
    return ((double)(z >> UNIFORM_SHIFT) + 0.5) * TWO_POW_NEG_53;
}

/* Inverse normal CDF (Acklam's approximation + one Halley step). */
static double inverse_normal_cdf(double p) {
    static const double a[] = { -3.969683028665376e+01,  2.209460984245205e+02,
        -2.759285104469687e+02,  1.383577518672690e+02,
        -3.066479806614716e+01,  2.506628277459239e+00 };
    static const double b[] = { -5.447609879822406e+01,  1.615858368580409e+02,
        -1.556989798598866e+02,  6.680131188771972e+01,
        -1.328068155288572e+01 };
    static const double c[] = { -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
         4.374664141464968e+00,  2.938163982698783e+00 };
    static const double d[] = {  7.784695709041462e-03,  3.224671290700398e-01,
         2.445134137142996e+00,  3.754408661907416e+00 };

    double x;
    if (p < PROB_LOW_TAIL) {
        double q = sqrt(-2.0 * log(p));
        x = (((((c[0]*q + c[1])*q + c[2])*q + c[3])*q + c[4])*q + c[5])
          / ((((d[0]*q + d[1])*q + d[2])*q + d[3])*q + 1.0);
    } else if (p > 1.0 - PROB_LOW_TAIL) {
        double q = sqrt(-2.0 * log(1.0 - p));
        x = -(((((c[0]*q + c[1])*q + c[2])*q + c[3])*q + c[4])*q + c[5])
          / ((((d[0]*q + d[1])*q + d[2])*q + d[3])*q + 1.0);
    } else {
        double q = p - 0.5, r = q * q;
        x = (((((a[0]*r + a[1])*r + a[2])*r + a[3])*r + a[4])*r + a[5]) * q
          / (((((b[0]*r + b[1])*r + b[2])*r + b[3])*r + b[4])*r + 1.0);
    }
    double e = 0.5 * erfc(-x / sqrt(2.0)) - p;
    double u = e * sqrt(TWO_PI) * exp(0.5 * x * x);
    return x - u / (1.0 + 0.5 * x * u);
}

static double gaussian(int sensor) {
    return inverse_normal_cdf(uniform_open(sensor));
}

/* Reference limiter, PI controller and duty cap.
   The measured battery current is m*i, and m depends on the error, so the
   loop is algebraic; it is solved exactly (PLECS breaks it with the tiny
   L_EMI inductor, which is effectively instantaneous). */
static Control controller(const State* s, double t, const Noise* n) {
    Control c;
    double vc_sense = s->vc + n->cap;
    c.i_min = -(vc_sense * vc_sense) / (IMIN_DIVISOR * V_G * R_TOTAL);
    c.i_ref = fmax(c.i_min, REF_AMPLITUDE * sin(REF_OMEGA * t));

    /* Feedforward duty from the quasi-static plant equation */
    c.m_ff = 0.0;
    if (FEEDFORWARD_ENABLED) {
        double discriminant = vc_sense * vc_sense + FF_QUAD_FACTOR * V_G * R_TOTAL * c.i_ref;
        c.m_ff = (vc_sense + sqrt(fmax(0.0, discriminant))) / (FF_ROOT_DIVISOR * V_G);
    }

    /* m = m_ff + kp*(i_ref - (m*i + noise)) + x  ->  solve for m */
    double m = (c.m_ff + KP_PI * (c.i_ref - n->bat) + s->x) / (1.0 + KP_PI * s->i);
    if (m > DUTY_MAX) m = DUTY_MAX;
    if (MPP_FLOOR_ENABLED) m = fmax(m, vc_sense / (FF_ROOT_DIVISOR * V_G));
    c.m = m;
    c.i_bat_sense = m * s->i + n->bat;
    c.error = c.i_ref - c.i_bat_sense;
    return c;
}

static State derivatives(const State* s, double t, const Noise* n) {
    Control c = controller(s, t, n);
    State d;
    d.i = (c.m * V_G - R_TOTAL * s->i - s->vc) / L_BLK;
    d.vc = s->i / C_TOTAL;
    d.x = KI_PI * c.error;
    return d;
}

static State add_scaled(const State* s, const State* d, double h) {
    State r = { s->i + h * d->i, s->vc + h * d->vc, s->x + h * d->x };
    return r;
}

static void step_rk4(State* s, double t, double h, const Noise* n) {
    State k1 = derivatives(s, t, n);
    State s2 = add_scaled(s, &k1, 0.5 * h);
    State k2 = derivatives(&s2, t + 0.5 * h, n);
    State s3 = add_scaled(s, &k2, 0.5 * h);
    State k3 = derivatives(&s3, t + 0.5 * h, n);
    State s4 = add_scaled(s, &k3, h);
    State k4 = derivatives(&s4, t + h, n);
    s->i += h * (k1.i + 2.0 * k2.i + 2.0 * k3.i + k4.i) / 6.0;
    s->vc += h * (k1.vc + 2.0 * k2.vc + 2.0 * k3.vc + k4.vc) / 6.0;
    s->x += h * (k1.x + 2.0 * k2.x + 2.0 * k3.x + k4.x) / 6.0;
}

int main(void) {
    const uint32_t tags[NUM_SENSORS] = { SENSOR_TAG_0, SENSOR_TAG_1 };
    uint64_t seed = SEED;
    if (seed == 0)
        seed = ((uint64_t)time(NULL) << SEED_TIME_SHIFT) ^ (uint64_t)clock();
    printf("Noise seed: %llu (set SEED to this value to repeat the run)\n",
        (unsigned long long)seed);
    for (int k = 0; k < NUM_SENSORS; k++)
        random_state[k] = seed ^ tags[k];

    FILE* out = fopen(OUTPUT_NAME, "w");
    if (!out) { perror("Cannot open CSV output"); return 1; }
    fprintf(out, "Time,Ibat,Ibat_sense,Iref,Imin,IL,Vc,M,Error,Mff\n");

    State s = { I_L0, V_C0, 0.0 };
    const long long steps = TOTAL_STEPS;
    const long long log_every = LOG_EVERY;

    for (long long n = 0; n <= steps; n++) {
        double t = (double)n * TIMESTEP;

        /* New noise sample every 1/fsw, held for the whole step */
        Noise noise = { 0.0, 0.0 };
        if (NOISE_ENABLED) {
            noise.bat = SIGMA_BAT * gaussian(SENSOR_BAT);
            noise.cap = SIGMA_CAP * gaussian(SENSOR_CAP);
        }

        if (n % log_every == 0) {
            Control c = controller(&s, t, &noise);
            fprintf(out, "%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
                t, c.m * s.i, c.i_bat_sense, c.i_ref, c.i_min, s.i, s.vc, c.m, c.error,
                c.m_ff);
        }
        if (n == steps) break;

        step_rk4(&s, t, TIMESTEP, &noise);
        if (!isfinite(s.i) || !isfinite(s.vc) || !isfinite(s.x)) {
            fprintf(stderr, "Non-finite state after %.6f s.\n", t);
            fclose(out);
            return 1;
        }
    }

    fclose(out);
    printf("Simulation complete. Data saved to %s\n", OUTPUT_NAME);
    return 0;
}