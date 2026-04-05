clear all
clc
%% Part 1: Frequency Selection
% We must first calculate our optimum switching frequency before optimizing
% magnetics design. We make the assumption that at optimal frequency,
% switching and conduction loss are equal. 
R_on = 0.36e-3;
I = 30;
RR = 0.2;
I_ms = I^2 + (RR*I)^2 / 3;
cond_loss = 2 * R_on * I_ms
% switching loss
Coss = 3300e-12;
v_l = 26;
f_sw = 120e3;
sw_loss = @(f_s) 2 * Coss * v_l^2 * f_s;
sw_loss(f_sw)
%% Part 2: Inductor Design
% We now design our inductor. We are using an ETD44 core with N97 ferrite.
% Unlike typical applications which may seek to choose a core size to
% optimize physical size, we wish to specifically optimize ESR at DC, which
% means picking the largest core available (ETD44). 
d_max = 9/16; % Maximum duty ratio
L = d_max * v_l/ (f_sw * RR * I) % Target inductance
I_max = (1+RR) * I; % Peak current
B_max = 390/1000; % Saturation B-field
safety_margin = 1.2;
A_c = 173e-6; % Core area
N_sat = L * I_max / (B_max * A_c) % # of turns to avoid saturation
%%
% We thus choose 11 turns. We now determine the air gap size to achieve the
% desired saturation and inductance characteristics. 
N = ceil(N_sat);
total_reluctance = N * I_max / (A_c * B_max * safety_margin)
l_c = 0.074;
mu_0 = 4 * pi * 1e-7;
mu_r = 2300;
core_reluctance = l_c / (mu_0 * mu_r * A_c);
gap_reluctance = total_reluctance - core_reluctance
gap_length = mu_0 * A_c * gap_reluctance;
disp(strcat("Gap size: ",string(gap_length * 1000)," mm"));