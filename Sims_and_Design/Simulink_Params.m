V_g = 24; % Nominal battery voltage
fsw = 120e3; % Switching frequency
V_c0 = 2; % Initial supercapacitor voltage
i_L0 = 0; % Initial inductor current

L_EMI = 27e-6; %minimum 1.5e-9
C_shunt = 10e-6;

R_on = 0.35e-3;   % On-state MOSFET resistance
R_l = 4.3e-3;   % Inductor ESR
R_esr = 18*10^-3; % Supercap resistance
R = 2 * R_on + R_l + R_esr/4;

L_blk = 28.306 * 10^-6; % Bulk inductance
L = L_blk;

C_sc = 110; % Supercap capacitance
C = 4*C_sc; % Total capacitance

tau_i = 1/(2*pi*fsw*1e-4); % Cutoff period
kp_pi = L/tau_i; % proportional gain [Ohms]
ki_pi = R/tau_i; % integral gain [F^-1]

dith_lim = 0.01;

%Pi-Circuit Model
W = fsw*1e-1; %frequency
Pr = 2*W*L_EMI;
Pc = 1/(W^2 * L_EMI);