function s = gate_scope_estimate(rise10to90_s,fall90to10_s,period_s,Cinput_F)
%GATE_SCOPE_ESTIMATE Diagnostic single-pole equivalents, not measured Rds(on).
% Valid only for a settled first-order gate-to-source waveform. Miller
% plateaus, moving source voltage and incomplete settling invalidate the fit.
% Use Ciss/effective input C, NEVER Coss. Omit Cinput_F for unknown R.
if nargin<4, Cinput_F=NaN; end
validateattributes(rise10to90_s,{'numeric'},{'scalar','positive','finite'});
validateattributes(fall90to10_s,{'numeric'},{'scalar','positive','finite'});
validateattributes(period_s,{'numeric'},{'scalar','positive','finite'});
assert(isscalar(Cinput_F) && (isnan(Cinput_F) || Cinput_F>0));
s = table(1/period_s,rise10to90_s/log(9),fall90to10_s/log(9), ...
    rise10to90_s/log(9)/Cinput_F,fall90to10_s/log(9)/Cinput_F, ...
    'VariableNames',{'Fsw_Hz','TauRise_s','TauFall_s', ...
    'ApparentGatePathRise_Ohm','ApparentGatePathFall_Ohm'});
end
