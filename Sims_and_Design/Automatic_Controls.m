%%
% This document serves as documentation for the design process for creating
% an automatic controls system for the supercapacitor system. 
clc;
clear all;
%% Exponential Characterization
data1 = [10 2.62702; 11 2.59799; 12 2.56928; 13 2.54088; 14 2.5128; ...
        15 2.48503; 16 2.45757; 17 2.43041; 18 2.40355; 19 2.37699; ...
        20 2.35072; 30 2.10348; 40 1.88225; 50 1.68428; 60 1.50714; ...
        70 1.34863; 80 1.20679; 90 1.07987; 100 0.966307; ...
        120 0.77375; 140 0.619578; 160 0.496126; 180 0.397279; 200 0.318132];

data2 = readmatrix('data2.csv');
x_2 = data2(:,1);
y_2 = data2(:,2);

x_1 = data1(:,1);
y_1 = data1(:,2);

f = @(p,x) p(1)*exp(-p(2)*x);
g = @(p,x) p(1)*exp(-p(2)*x);
% objective = squared error
err_1 = @(p) sum((f(p,x_1)-y_1).^2);
err_2 = @(p) sum((g(p,x_2)-y_2).^2);

p0 = [2 0.01];   % initial guess
p_1 = fminsearch(err_1,p0);
p_2 = fminsearch(err_2,p0);

% plot
grid on
%plot(x_1,y_1,'o')
%plot(x_2,y_2,'x')
hold on

xx_1 = linspace(min(x_1),max(x_1),200);
xx_2 = linspace(min(x_2),max(x_2),200);
plot(xx_1,f(p_1,xx_1),'LineWidth',2)
plot(xx_2,g(p_2,xx_2),'LineWidth',2)

mse1 = mean((f(p_1,x_1)-y_1).^2);
mse2 = mean((g(p_2,x_2)-y_2).^2);
fprintf(['MSE1 = %.10f\n'], mse1)
fprintf('y1 = %.4f * exp(-%.5f x)\n',p_1(1),p_1(2))
fprintf(['MSE2 = %.10f\n'], mse2)
fprintf('y2 = %.4f * exp(-%.5f x)\n',p_2(1),p_2(2))
legend('Fit 1','Fit 2')