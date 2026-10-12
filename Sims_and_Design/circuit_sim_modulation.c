#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PI 3.14159265358979323846

int main(void) {

    const double timestep = 1e-5;
    const double C = 440;
    const double R = 0.0095;
    const double L = 28.3e-6;
    const double Vg = 24;
    const double fsw = 120e3;

    const double power = 90;
    const double i_ref = power / Vg;
    
    const double total_time = 5 * R * C;
    const int steps = (int)(round(total_time / timestep));

    const int save_int = 100; //Save one row per 1 ms instead of every step

    double vc_n = 0;
    double i_n = 0;
    double m_n = 0;
    double mt_n = 0;
    double ibat_n = 0;
    
    double error_prev = i_ref - ibat_n;

    const double tau_i = 1 / (2 * PI * fsw * 1e-4);

    const double kp_pi = L / (Vg * tau_i);
    const double ki_pi = R / (Vg * tau_i);

    FILE *output = fopen("circuit_data.csv", "w");

    if (output == NULL) {
        perror("Error opening file");
        return 1;
    }

    fprintf(output,
        "Time,Vc,I,Ibat,Iref,Power,M,MT\n");

    for (int n = 0; n <= steps; n++) {
        double current_time = n * timestep;
        ibat_n = m_n * i_n;
        double error = i_ref - ibat_n;
        double mt_n1 = mt_n + kp_pi * (error - error_prev) 
                       + (timestep * ki_pi / 2) * (error + error_prev);
        
        double kt_n = vc_n / (2 * Vg);

        double sqrt_term = mt_n1 + kt_n * kt_n;

        if (sqrt_term < 0) {
            sqrt_term = 0;
        }
        double m_n1 = kt_n + sqrt(sqrt_term);
        m_n = fmax(0, fmin(1, m_n1));

        if (m_n != m_n1) {
            mt_n = m_n * m_n - 2 * kt_n * m_n;
        } else {
            mt_n = mt_n1;
        }

        error_prev = error;

        double ibat_applied = m_n * i_n;
        double power = Vg * ibat_applied;

        if (n % save_int == 0 || n == steps) {

            fprintf(output,
                "%.6f,%.6f,%.6f,%.6f,%.6f,"
                "%.6f,%.6f,%.6f\n",
                current_time,
                vc_n,
                i_n,
                ibat_applied,
                i_ref,
                power,
                m_n,
                mt_n
            );

            printf(
                "Time: %.3f s | "
                "Vc: %.4f V | "
                "Ibat: %.4f A | "
                "Power: %.2f W | "
                "M: %.4f\n",
                current_time,
                vc_n,
                ibat_applied,
                power,
                m_n
            );
        }

        double vc_n1 = vc_n + timestep * (i_n) / C;
        double i_n1 = i_n + timestep * ((m_n * Vg - R*i_n- vc_n) / L);

        vc_n = vc_n1;
        i_n = i_n1;

    }

    fclose(output);

    printf("\nSimulation complete!\n");
    printf("Data saved to circuit_data.csv\n");

    printf("\nFinal values:\n");
    printf("Capacitor voltage: %.6f V\n", vc_n);
    printf("Inductor current: %.6f A\n", i_n);
    printf("Battery current: %.6f A\n", m_n * i_n);
    printf("Battery power: %.6f W\n", Vg * m_n * i_n);
    printf("Modulation: %.6f\n", m_n);

    return 0;
}