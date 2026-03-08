%%
% This document serves as documentation for the design process for creating
% an automatic controls system for the supercapacitor system. 
clc;
clear all;
%% Part 1: Initial Exponential Characterization
% After creating a switched-cycle model for the bidirectional converter,
% testing revealed significant nonlinearities that resulted in divergent
% behavior when used in a linearized model. Inputting a step function 
% appears to output a decaying exponential, implying potential convergence 
% on a closed-form expression for the transfer function. However, inputting
% a ramp input yields a parabola. Furthermore, multiplying an input by a 
% scalar factor does not result in a commensurate scaling of the output
% behavior. We thus conjecture that usage of a square-root function can
% account for nonlinearities of the system. 
data = [10 2.62702; 11 2.59799; 12 2.56928; 13 2.54088; 14 2.5128; ...
        15 2.48503; 16 2.45757; 17 2.43041; 18 2.40355; 19 2.37699; ...
        20 2.35072; 30 2.10348; 40 1.88225; 50 1.68428; 60 1.50714; ...
        70 1.34863; 80 1.20679; 90 1.07987; 100 0.966307; ...
        120 0.77375; 140 0.619578; 160 0.496126; 180 0.397279; 200 0.318132];
% test data from step input using square-root correction
x = data(:,1);
y = data(:,2);

f = @(p,x) p(1)*exp(p(2)*x);

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
fprintf('MSE = %.10f\n', mse)
fprintf('y = %.4f * exp(%.5f x)\n',p(1),p(2))
% Add labels and title to the plot
xlabel('Input Voltage (V)');
ylabel('Output Current (A)');
title('Exponential Characterization of Supercapacitor System');
legend('Data Points', 'Fitted Curve', 'Location', 'Best');
hold off
%%
% Thanks to Timothy, Nan, and Ethan for helping with characterization!
%% Part 2: Voltage + Current Relations
% We can see that inputting a step function yields output behavior closely
% modeling that of a decaying exponent. However, this is insufficient to
% characterize the square-root corrected system as linear, as this is still
% only a single step function. Furthermore, this result does not elucidate
% us on expected behavior of capacitor voltage, which is another state 
% variable we wish to measure. We will thus repeat Step 1 on a suite of
% inputs to see if reverse-engineering of their behavior is possible. 
warning('off', 'MATLAB:table:ModifiedAndSavedVarnames');
step_data = readtable("1_16_step.csv");
step_time = transpose(0 : 0.5 : (size(step_data, 1) - 1) / 2);
step_input = step_data.Abs;
step_outputCurrent = step_data.AverageCurrent;
step_capacitorVoltage = step_data.C1_CapacitorVoltage;
ramp_data = readtable("1_1600_ramp.csv");
ramp_time = ramp_data.Time_S;
ramp_inputDuty = ramp_data.Abs;
ramp_outputCurrent = ramp_data.AverageCurrent;
ramp_capacitorVoltage = ramp_data.C1_CapacitorVoltage;
%%
% Let's re-run some fitting code and make sure that everything still works
% as expected. 
[xData, yData] = prepareCurveData( step_time, step_outputCurrent );

% Set up fittype and options.
ft = fittype( 'exp1' );
excludedPoints = yData <= 0;
opts = fitoptions( 'Method', 'NonlinearLeastSquares' );
opts.Display = 'Off';
opts.StartPoint = [0.0179102790083968 -0.0106859410802364];
opts.Exclude = excludedPoints;

% Fit model to data.
[fitresult, gof] = fit( xData, yData, ft, opts );

% Plot fit with data.
figure( 'Name', 'ramp response' );
h = plot( fitresult, xData, yData, excludedPoints );
legend( h, 'step_outputCurrent vs. step_time', 'Excluded step_outputCurrent vs. step_time', 'ramp response', 'Location', 'NorthEast', 'Interpreter', 'none' );
% Label axes
xlabel( 'step_time', 'Interpreter', 'none' );
ylabel( 'step_outputCurrent', 'Interpreter', 'none' );
grid on
%%
% Looks pretty similar! We can use the Laplace transform of this input and 
% output to derive a transfer function. 
input_s = tf(1, [16 0]); % Yes, using a transfer function to represent a step input is "wrong". Sue me
output_s = tf(fitresult.a, [1 -fitresult.b]);
G_id = output_s / input_s;
figure;
lsim(10 * G_id, step_input(1:end), step_time(1:end));
hold on;
plot(step_time(1:end), step_outputCurrent(1:end));
%%
% Not sure why there's a stray factor of 10. We'll keep going and see what
% happens.
corrected_ramp_data = readtable("0.05_6400_ramp.csv");
corrected_ramp_time = corrected_ramp_data.Time_S;
corrected_ramp_inputDuty = corrected_ramp_data.Math;
corrected_ramp_outputCurrent = corrected_ramp_data.AverageCurrent;
corrected_ramp_capacitorVoltage = corrected_ramp_data.C1_CapacitorVoltage;
figure;
lsim(10 * G_id, corrected_ramp_inputDuty(1:end), corrected_ramp_time(1:end));
hold on;
plot(corrected_ramp_time(1:end), corrected_ramp_outputCurrent(1:end), "Color", "r");
legend;