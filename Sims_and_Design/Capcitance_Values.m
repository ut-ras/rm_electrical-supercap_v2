clc
clear all;
%% Input Capacitance
% Assume all ripple current goes through the capacitor and calculate
% capacitance to limit voltage rise to acceptable level. 
I = 3.75; % Maximum steady state input current
V = 26; % Maximum DC link voltage
max_V = 30; % Breakdown voltage of MOSFETs
delta_V = max_V - V;
f_sw = 120e3; % switching frequency
d_max = 9/16; 
safety_margin = 1.2;
I * d_max * safety_margin / (delta_V * f_sw)