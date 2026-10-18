#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

int main(void) {
    const double timestep = 1e-5;
    const double C = 440.0;
    const double R = 0.0095;
    const double L = 28.3e-6;
    const double Vg = 24.0;

    const double power_ref = 90.0;
    const double ibat_ref = 0; //power_ref / Vg;
    const double freq_HZ = 100.0;
    const double load_amplitude = 1.0;

    const double total_time = 5.0 * R * C;
    const int steps = (int)round(total_time / timestep);
    const int save_int = 10;

    double vc_n = 1.5;
    double i_n = 0.0;
    double m_n = vc_n / Vg;
    double mt_n = m_n * m_n - (vc_n / Vg) * m_n;
    double ibat_n = ibat_ref;
    double error_prev = 0.0;

    const double controller_Hz = 1200.0;
    const double tau_i = 1.0 / (2.0 * PI * controller_Hz);
    const double kp_pi = L / (Vg * tau_i);
    const double ki_pi = R / (Vg * tau_i);

    FILE* output = fopen("circuit_data.csv", "w");

    if (output == NULL) {
        perror("Error opening file");
        return 1;
    }

    fprintf(
        output,
        "Time,Vc,I,Iref,Iconverter,Ibat,IbatRef,"
        "Power,LoadPower,PowerRef,M,MT\n"
    );

    for (int n = 0; n <= steps; n++) {
        double current_time = n * timestep;

        double i_ref = ibat_ref
            + load_amplitude * sin(2.0 * PI * freq_HZ * current_time);

        double i_converter = m_n * i_n;
        ibat_n = i_ref + i_converter;

        double error = i_ref - ibat_n;

        double mt_n1 = mt_n
            + kp_pi * (error - error_prev)
            + (timestep * ki_pi / 2.0) * (error + error_prev);

        double kt_n = vc_n / (2.0 * Vg);
        double sqrt_term = fmax(0.0, mt_n1 + kt_n * kt_n);

        double m_n1 = kt_n + sqrt(sqrt_term);
        m_n = fmax(0.0, fmin(1.0, m_n1));

        mt_n = m_n * m_n - 2.0 * kt_n * m_n;
        error_prev = error;

        i_converter = m_n * i_n;
        ibat_n = i_ref + i_converter;

        double battery_power = Vg * ibat_n;
        double load_power = Vg * i_ref;

        if (n % save_int == 0 || n == steps) {
            fprintf(
                output,
                "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,"
                "%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",
                current_time,
                vc_n,
                i_n,
                i_ref,
                i_converter,
                ibat_n,
                ibat_ref,
                battery_power,
                load_power,
                power_ref,
                m_n,
                mt_n
            );
        }

        if (n == steps) {
            break;
        }

        double vc_n1 = vc_n + timestep * i_n / C;
        double i_n1 = i_n
            + timestep * (m_n * Vg - R * i_n - vc_n) / L;

        vc_n = vc_n1;
        i_n = i_n1;
    }

    if (fclose(output) != 0) {
        perror("Error closing file");
        return 1;
    }

    printf("Simulation complete. Data saved to circuit_data.csv\n");
    printf("Opening plots...\n");
    fflush(stdout);

    if (system("py -3.13 plot_circuit.py") != 0) {
        fprintf(
            stderr,
            "Plotting failed. Run python plot_circuit.py "
            "from this folder to see the error.\n"
        );
        return 1;
    }

    //if issues with python, cd the folder into Sims_and_design, the run these two lines: 1) py -3.13 -m pip install matplotlib and 2) py -3.13 plot_circuit.py

    return 0;
}