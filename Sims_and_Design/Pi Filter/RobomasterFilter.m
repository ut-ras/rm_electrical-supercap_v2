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

%% Section 2: Egan's Design
% We begin with our inductor and expected noise frequencies. Our switching
% frequency should be approximately 150kHz, so we should expect harmonics
% at 150KHz and the associated higher harmonics. We also have already
% chosen an inductor to use with the converter. The previous design used a
% 47uH inductor, but that inductor value was chosen simply by winding a
% core with a wire as much as possible and seeing what the resultant
% inductance is. Now, we will buy two of the converter inductor and simply
% design our capacitance and resistance to match. 
L = 33e-6;
f_sw = 150e3;
%%
% Let's set the attenuation of the zeroth harmonic of the switching
% frequency at -80dB. This means that at -40 dB/decade, our cutoff
% frequency is 2 decades earlier. We can solve for capacitance through
% 2*pi*f_c = 1/sqrt(LC).
f_c = 1.5e2;
C = 1/((2*pi*f_c)^2*L)
% C = 1000e-6;
% L = 1/((2*pi*f_c)^2*C)
%%
% We can now design the RLC filter. Let's find a suitable capacitor and 
% add the ESRs of the components.
C = 1200e-6; % Part # ESK477M050AH4KA
C = 12000e-6;
R_L = 1.33e-3 + 1.2e-3;
R_C = 11e-3; % part # ESK477M050AH4KA
%%
% Now we can sweep for a resistance (there's an analytical way to do it,
% but it's a pain in the ass).
s = tf("s");
R_s = 0.5; % sweep this value
h_filter = (R_C + R_s + 1/(s*C)) / (R_C + R_s + R_L + (s * L) + 1/(s*C));
Z_1 = R_L + s*L;
Z_2 = R_s + R_C + 1/(s * C);
% h_filter2 = ((Z_2)^2)/(Z_1^2 + 3 * Z_1 * Z_2 + 2* Z_2^2);
bode(h_filter, {1e2, 2*pi*f_sw*1e2});
% hold on;
% bode(h_filter2, {1e2, 2*pi*f_sw*1e2});
% legend;