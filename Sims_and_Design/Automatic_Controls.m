%%
% This document serves as documentation for the design process for creating
% an automatic controls system for the supercapacitor system. 
clc;
clear all;
%% Exponential Characterization
data = [10 2.62702; 11 2.59799; 12 2.56928; 13 2.54088; 14 2.5128; ...
        15 2.48503; 16 2.45757; 17 2.43041; 18 2.40355; 19 2.37699; ...
        20 2.35072; 30 2.10348; 40 1.88225; 50 1.68428; 60 1.50714; ...
        70 1.34863; 80 1.20679; 90 1.07987; 100 0.966307; ...
        120 0.77375; 140 0.619578; 160 0.496126; 180 0.397279; 200 0.318132];

x = data(:,1);
y = data(:,2);

f = @(p,x) p(1)*exp(-p(2)*x);

% objective = squared error
err = @(p) sum((f(p,x)-y).^2);

p0 = [2 0.01 0];   % initial guess
p = fminsearch(err,p0);

% plot
grid on
plot(x,y,'o')
hold on

xx = linspace(min(x),max(x),500);
plot(xx,f(p,xx),'LineWidth',2)

mse = mean((f(p,x)-y).^2);
fprintf(['MSE = %.10f\n'], mse)
fprintf('y = %.4f * exp(-%.5f x)\n',p(1),p(2))