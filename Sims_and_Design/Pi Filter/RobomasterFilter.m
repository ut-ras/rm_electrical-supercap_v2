% UT Robomasters - Supercap Power Stage Choke/Filter Design
% Topology: 2nd Order RLC Filter (Series L, Shunt R-C Branch)

clear; clc; close all;

%% Target
f_c = 12;
omega_n = 2 * pi * f_c;

%% Components
% I think we need to build the inductor then pick a C 
L = 0.005; % PUT IN VALUE FROM LCR METER
C = 1 / (L * omega_n^2); % standard equation for Capacatance of LC lowpass

% damping resistor
R = 0.5; % THIS CAN CHANGE TO GET DESIRED RESPONCE/damping ratio

%% Damping Ratio
zeta = (R/2) * sqrt(C / L);
Q = 1 / (2 * zeta);


%% Transfer Function
s = tf('s');
% Impedance of shunt RC
Z_shunt = R + 1/(s*C);
% inductor impedance
Z_series_ideal = s*L;
H_ideal = Z_shunt / (Z_series_ideal + Z_shunt);


%% Plots
zero_freq_hz = 1 / (2 * pi * R * C);
pole_freq_hz = f_c;

fprintf('--- Filter Characteristics ---\n');
fprintf('Target Pole Frequency: %.2f Hz\n', pole_freq_hz);
fprintf('Actual Zero Frequency: %.2f Hz\n', zero_freq_hz);
fprintf('Damping Ratio (zeta): %.4f\n', zeta);
fprintf('Quality Factor (Q): %.4f\n', Q);

% Plotting
figure('Name', 'Supercap Power Stage Filter Analysis', 'Position', [100, 100, 1000, 400]);

% Plot 1: Bode Plot
subplot(1, 2, 1);
bode(H_ideal, 'b');
grid on;
title('Bode Plot (Frequency Response)');

% Plot 2: Pole-Zero Map
subplot(1, 2, 2);
pzmap(H_ideal, 'b');
grid on;
title('Pole-Zero Map');