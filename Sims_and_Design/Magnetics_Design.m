clear all
clc
%%
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
%% 
%
d_max = 9/16;
L = d_max * v_l/ (f_sw * RR * I)
I_max = (1+RR) * I;
B_max = 390/1000;
safety_margin = 1.2;
A_c = 76e-6;
N = 8;
total_reluctance = N * I_max / (A_c * B_max * safety_margin)
l_c = 0.074;
mu_0 = 4 * pi * 1e-7;
mu_r = 1650;
core_reluctance = l_c / (mu_0 * mu_r * A_c);
gap_reluctance = total_reluctance - core_reluctance
gap_length = mu_0 * A_c * gap_reluctance;
real_gap_length = 0.5 * gap_length