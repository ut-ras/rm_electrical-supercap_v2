function c = loss_defaults()
%LOSS_DEFAULTS Explicit screening assumptions; all quantities SI.
c.Vbus_V=24; c.Vcap_V=3; c.IL_A=30; c.rippleRatio=0.2;
c.frequencyMode="per_point"; % "envelope": max required f across sampled V/I
c.maxConductionDropFraction=0.10; % Heuristic validity screen for ideal duty
c.mode="buck_charge"; c.extraSeriesFETs=1;
c.VbusMax_V=26; c.overshoot_V=6; c.voltageRatingFactor=1.2;
c.inductorTemp_C=80; c.copperAlpha_perC=0.00393;
c.rdsTemperatureFactor=1.5; c.Lfraction=1;
c.minFsw_Hz=1e3; c.maxFsw_Hz=500e3; % User design limits, NOT driver ratings
c.fswFloor_Hz=0; % Raise f to at least this (e.g. 25e3, above audible); 0=off
c.capSeries=1; c.capParallel=4; c.capEachESR_Ohm=18e-3;
c.capEach_F=110; c.capEachRated_V=3.1; c.capEachIrms_A=NaN;
c.bufferESR_Ohm=NaN;
c.Vdrive_V=12; % Schematic nominal; scope-test local VCC not measured
c.RgateExternal_Ohm=0.75; % Confirmed fitted; bypass reportedly unchanged
% NCP5109 p5 maximum output resistance; peak currents are TYPICAL only.
% Specified at 15 V. Extrapolation to 12 V is screening, not guaranteed.
c.Rsource_Ohm=60; c.Rsink_Ohm=20;
c.IpeakSource_A=0.250; c.IpeakSink_A=0.500;
c.propagation_s=170e-9; c.minPulse_s=50e-9;
c.deadtime_s=100e-9; c.maxGatePulseFraction=0.25;
c.driverOverhead_W=NaN; % Additional unloaded driver/supply loss; avoid
% adding the datasheet's loaded ICC to Qg*V*f (would double-count load).
c.powerBudget_W=90;
end
