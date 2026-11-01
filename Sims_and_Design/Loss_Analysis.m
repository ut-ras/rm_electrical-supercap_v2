%% Spreadsheet-driven supercapacitor converter loss screening
% Run this script. Results and imported tables remain in the workspace.
% See Loss_Analysis_Notes.txt for equations, units, limitations and sources.
% This is a CCM screening model, not a thermal or switching simulation.
baseDir = fileparts(mfilename('fullpath'));
addpath(baseDir);
cfg = loss_defaults();

%% EDIT operating point and assumptions here
cfg.Vbus_V = 24;
requestedCapVoltages_V = 0:0.25:3; % Confirmed 0-3 V range, sampled for screening
cfg.Vcap_V = requestedCapVoltages_V(requestedCapVoltages_V > 0);
% Zero volts is a startup transient, not a steady-state CCM operating point.
% Vcap below is loaded TERMINAL voltage, not internal/open-circuit voltage.
cfg.IL_A = 30;                   % Assumed current limit, NOT constant 90 W
cfg.frequencyMode = "envelope"; % One frequency per L for all sampled V/I
% Set "per_point" to solve independently for exactly 20% at every point.
cfg.rippleRatio = 0.20;          % PEAK-TO-PEAK / average inductor current
cfg.fswFloor_Hz = 0;             % Try 25e3: 20%-ripple f is ~8-30 kHz (audible)
cfg.mode = "buck_charge";       % Or "boost_discharge" (same low-side IL)
cfg.extraSeriesFETs = 1;         % Confirmed four-switch bidirectional stage
% Assumes one PWM half bridge and one held-on switch. A held high-side
% bootstrap driver needs refresh or an independent floating supply.
cfg.VbusMax_V = 26;              % ASSUMPTION: replace with measured/spec max
cfg.overshoot_V = 6;             % ASSUMPTION: replace with measured Vds ringing
cfg.voltageRatingFactor = 1.20;  % Applied to (VbusMax + overshoot): 38.4 V
cfg.inductorTemp_C = 80;         % ASSUMPTION, no thermal solution
cfg.rdsTemperatureFactor = 1.5;  % ASSUMPTION: replace from selected FET curve
cfg.Lfraction = 1;              % Nominal L only; reduce for tolerance/DC bias
cfg.Vdrive_V = 12;              % Schematic nominal; verify VCC at the IC
% Reference Qg/Rds are specified at 10 V. Retained as flagged estimates;
% Qg at 12 V is not established by the supplied 10 V table entry.
cfg.RgateExternal_Ohm = 0.75;   % Confirmed fitted value; use 0 for bypass test
cfg.deadtime_s = 100e-9;         % ASSUMPTION for firmware; NCP5109A has none!
cfg.capSeries = 1;
cfg.capParallel = 4;            % Confirmed four cells in parallel
cfg.capEachESR_Ohm = 18e-3;
cfg.capEach_F = 110;
cfg.capEachRated_V = 3.1;       % User-stated cell ceiling; use 3 V operation
cfg.capEachIrms_A = NaN;        % REQUIRED before approving capacitor heating
cfg.bufferESR_Ohm = NaN;        % Effective high-voltage bus capacitor ESR
% Unknown core/AC winding/buffer/driver overhead stays NaN, not zero.

% Add more vendor exports to these lists; every inductor meets every MOSFET.
inductorFiles = fullfile(baseDir,'Loss_Data',{'Power_Inductor.xlsx', ... % Coilcraft
    'Wurth_Power_Inductors.xlsx'});                                      % Wurth catalog
mosfetVendorFiles = fullfile(baseDir,'Loss_Data',{'Mouser_MOSFETs.csv'});
% Datasheet table (canonical SI columns) supplies the dynamic data Mouser
% lacks and overrides Mouser's typical Qg; see merge_mosfet_data.m.
mosfetDatasheetFile = fullfile(baseDir,'Loss_Data','MOSFET_candidates.csv');
inductorTables = cellfun(@(f) import_loss_parts(f,"inductor"),inductorFiles,'UniformOutput',false);
inductors = vertcat(inductorTables{:});
[~,firstRow] = unique(inductors.PartNumber,'stable');
if numel(firstRow)<height(inductors)
    warning('loss:DuplicateInductor','Duplicate inductor part numbers; keeping first file''s row.');
    inductors = inductors(firstRow,:);
end
mosfetTables = cellfun(@(f) import_loss_parts(f,"mosfet"),mosfetVendorFiles,'UniformOutput',false);
mosfetVendor = vertcat(mosfetTables{:});
mosfets = merge_mosfet_data(mosfetVendor,import_loss_parts(mosfetDatasheetFile,"mosfet"));
results = converter_losses(inductors,mosfets,cfg);
results = sortrows(results,{'ScreenFailures','KnownLossSubtotal_W'});
% Aggregate across voltages so a favorable single point cannot hide failures.
[group,inductorNames,mosfetNames]=findgroups(results.Inductor,results.MOSFET);
componentSummary=table(inductorNames,mosfetNames, ...
    splitapply(@max,results.Fsw_Hz,group), ...
    splitapply(@max,results.KnownLossSubtotal_W,group), ...
    splitapply(@max,results.ScreenFailures,group), ...
    splitapply(@(v) any(v>0),results.UnknownLossTerms,group), ...
    splitapply(@(s) all(s=="SCREEN_ONLY"),results.Screen,group), ...
    splitapply(@min,results.GateTimingOK,group), ...
    splitapply(@failedScreens,results.Notes,group), ...
    'VariableNames',{'Inductor','MOSFET','MaxFsw_Hz','MaxKnownLossSubtotal_W', ...
    'WorstScreenFailures','HasUnknownLoss','AllSampledPointsScreenOnly', ...
    'WorstGateTimingOK','FailedScreens'});
[~,loc]=ismember(componentSummary.Inductor,inductors.PartNumber);
componentSummary.InductorSource=inductors.Source(loc);
componentSummary=sortrows(componentSummary,{'WorstScreenFailures','MaxKnownLossSubtotal_W'});
outDir = fullfile(baseDir,'Loss_Results');
if ~isfolder(outDir), mkdir(outDir); end
writetable(results,fullfile(outDir,'component_comparison.csv'));
writetable(componentSummary,fullfile(outDir,'component_summary.csv'));
writetable(inductors,fullfile(outDir,'imported_inductors.csv'));
writetable(mosfets,fullfile(outDir,'imported_mosfets.csv'));
disp(componentSummary(1:min(15,height(componentSummary)),:));
fprintf('\n%d combinations exported to %s\n',height(results),outDir);
fprintf('Subtotal omits unknown losses. SCREEN_ONLY is not hardware approval.\n');
fprintf('30 A at 3 V is 90 W at the capacitor terminals; ESR reduces stored power.\n');
fprintf('0 V excluded: use a current-limited startup model; zero-voltage boost cannot deliver power.\n');
fprintf('Frequency is selected over sampled points; low-current operation needs a separate sweep.\n');

%% Scope diagnostics: tek00001.webp displayed measurements
scope = gate_scope_estimate(2.087e-6,777.1e-9,4.180e-6,14e-9);
% 14 nF is TYPICAL Ciss of the confirmed part, not constant effective gate C.
% Confirmed low-side Vgs, with 0.75 ohm external R. User reports essentially
% unchanged waveform with R shorted. Schematic VCC is 12 V; local test
% supply waveform has not been measured/confirmed.
disp(scope);
% RC sensitivity only: bypassing R removes ln(9)*R*C from a 10-90% edge.
% This is not a fit to the nonlinear, incompletely settled captured waveform.
inputC_F=[14e-9;18e-9]; % Typical/max Ciss at the datasheet test bias
gateBypassEstimate=table(inputC_F*1e9, ...
    log(9)*cfg.RgateExternal_Ohm*inputC_F*1e9, ...
    'VariableNames',{'AssumedInputCapacitance_nF','Predicted10to90Reduction_ns'});
disp(gateBypassEstimate);
writetable(gateBypassEstimate,fullfile(outDir,'gate_resistor_bypass_estimate.csv'));

function s=failedScreens(notes)
% Union of screen labels that failed at any sampled point.
hits=regexp(strjoin(notes,' '),'(?<=FAIL: )[^.]*','match');
s=strjoin(unique(strtrim(split(strjoin(string(hits),', '),', '))),', ');
if isempty(hits), s=""; end
end
