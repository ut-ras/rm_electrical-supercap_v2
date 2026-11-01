function test_loss_analysis()
%TEST_LOSS_ANALYSIS Numerical regression, import units and missing-data tests.
root=fileparts(mfilename('fullpath'));
L=import_loss_parts(fullfile(root,'Loss_Data','Power_Inductor.xlsx'),"inductor");
F=import_loss_parts(fullfile(root,'Loss_Data','MOSFET_candidates.csv'),"mosfet");
assert(height(L)>100);
assert(L.PartNumber(1)=="XGL3020-101");
near(L.L_H(1),0.1e-6); near(L.DCR_Ohm(1),2e-3);
near(L.Isat_A(1),14.8); near(L.Irms_A(1),28);
near(F.Qg_C(1),262e-9);
l=L(1,:); l.L_H=33e-6; l.DCR_Ohm=4.3e-3;
l.Isat_A=40; l.Irms_A=35; l.CoreLoss_W=0; l.ACWindingLoss_W=0;
m=F(1,:); m.Vds_V=40;
c=loss_defaults(); c.inductorTemp_C=25; c.rdsTemperatureFactor=1;
c.driverOverhead_W=0; c.bufferESR_Ohm=0;
c.capEachRated_V=3; c.capEachIrms_A=10;
r=converter_losses(l,m,c);
near(r.Fsw_Hz,13257.575757575758);
near(r.DeltaIpp_A,6); near(r.Ipeak_A,33); near(r.Irms_A^2,903);
near(r.PinductorDC_W,3.8829); near(r.Psupercap_W,4.0635);
near(r.PfetConduction_W,0.6321); near(r.Pconduction_W,8.5785);
near(c.Vdrive_V,12);
near(r.PgateSupply_W,2*262e-9*12*r.Fsw_Hz);
assert(contains(r.Notes,"Rds/Qg gate-voltage conditions differ"));
c10=c; c10.Vdrive_V=10;
r10=converter_losses(l,m,c10);
near(r.PgateSupply_W,1.2*r10.PgateSupply_W); % Qg held fixed: estimate only
assert(r.OverlapOn_s<r10.OverlapOn_s);
near(r.ModelledTotal_W,r.KnownLossSubtotal_W);
near(r.RequiredBusPower_W,90+r.ModelledTotal_W-r.Psupercap_W);
near(r.StoredPower_W,90-r.Psupercap_W);
assert(r.VoltageOK==1 && r.BudgetKnownExceeded==1);
l.L_H=330e-6;
r330=converter_losses(l,m,c); near(r330.Fsw_Hz,1325.757575757576);
c.extraSeriesFETs=0;
r2=converter_losses(l,m,c); near(r2.PfetConduction_W,r.PfetConduction_W/2);
c.mode="boost_discharge";
rb=converter_losses(l,m,c); near(rb.Fsw_Hz,r330.Fsw_Hz);
near(-rb.StoredPower_W-rb.DeliveredBusPower_W,rb.ModelledTotal_W);
c.mode="buck_charge"; c.Vcap_V=[3,6]; c.IL_A=[30,15];
assert(height(converter_losses(l,m,c))==2);
c.Vcap_V=3; c.IL_A=30;
l.CoreLoss_W=NaN;
ru=converter_losses(l,m,c);
assert(isnan(ru.ModelledTotal_W) && ru.UnknownLossTerms==1);
l.CoreLoss_W=0; m.Qg_C=NaN;
ru=converter_losses(l,m,c);
assert(isnan(ru.GateTimingOK) && isnan(ru.ModelledTotal_W));
m=F(1,:); m.Eon_J=10e-6; m.Eoff_J=5e-6;
rm=converter_losses(l,m,c);
near(rm.Pswitching_W,15e-6*rm.Fsw_Hz);
near(rm.ModelledTotal_W,rm.Pconduction_W+rm.Pdeadtime_W+rm.PgateSupply_W+rm.Pswitching_W);
assert(rm.VoltageOK==0);
% Gate charge/RC screen must reject the same FET at a much higher frequency.
l.L_H=1e-6;
fast=converter_losses(l,m,c); assert(fast.GateTimingOK==0);
% A single frequency must satisfy every sampled V/I, with ripple recomputed.
l.L_H=33e-6; c.frequencyMode="envelope";
c.Vcap_V=[0.25,1,3]; c.IL_A=30;
envelope=converter_losses(l,m,c);
assert(all(abs(envelope.Fsw_Hz-13257.575757575758)<1e-8));
assert(all(envelope.ActualRippleRatio<=c.rippleRatio+1e-12));
near(envelope.ActualRippleRatio(1),0.2*(0.25*23.75)/(3*21));
near(envelope.Irms_A(1)^2,900+envelope.DeltaIpp_A(1)^2/12);
assert(envelope.IdealDutyApproximationOK(1)==0);
assert(envelope.IdealDutyApproximationOK(3)==1);
c.IL_A=[30,30,1];
lowCurrent=converter_losses(l,m,c);
near(lowCurrent.Fsw_Hz(1),30*envelope.Fsw_Hz(1));
% Frequency floor raises f above the ripple minimum and reduces ripple.
c.IL_A=30; c.fswFloor_Hz=50e3;
floored=converter_losses(l,m,c);
assert(all(floored.Fsw_Hz==50e3));
near(floored.ActualRippleRatio(3),0.2*13257.575757575758/50e3);
c.fswFloor_Hz=0;
c.Vcap_V=0;
failed=false;
try
    converter_losses(l,m,c);
catch e
    failed=strcmp(e.identifier,'loss:Startup');
end
assert(failed,'Zero volts must be explicitly rejected as a startup condition.');
s=gate_scope_estimate(2.087e-6,777.1e-9,4.180e-6,14e-9);
near(s.TauRise_s,2.087e-6/log(9)); near(s.Fsw_Hz,1/4.180e-6);
% Unit-bearing vendor cells; SI headers; metadata before header; rejection.
file=[tempname,'.csv']; cleanup=onCleanup(@() delete(file));
writecell({'export','','';'Part Number','Inductance','DC Resistance'; ...
    'unit-test','33 uH','4.3 mOhms'},file);
u=import_loss_parts(file,"inductor"); near(u.L_H,33e-6); near(u.DCR_Ohm,4.3e-3);
writecell({'PartNumber','L_H','DCR_Ohm';'si-test',33e-6,4.3e-3},file);
u=import_loss_parts(file,"inductor"); near(u.L_H,33e-6);
writecell({'Part Number','Inductance','DC Resistance';'bad',33,4.3},file);
failed=false;
try
    import_loss_parts(file,"inductor");
catch e
    failed=strcmp(e.identifier,'loss:Unit');
end
assert(failed,'Ambiguous unitless vendor values must be rejected.');
writecell({'Part Number','L (uH)','RDC typ (mOhm)';'vendor',33,4.3},file);
u=import_loss_parts(file,"inductor"); near(u.L_H,33e-6); near(u.DCR_Ohm,4.3e-3);
% Wurth catalog export: numeric order codes, unit-free SI cells, IR=Irms.
W=import_loss_parts(fullfile(root,'Loss_Data','Wurth_Power_Inductors.xlsx'),"inductor");
assert(height(W)==16 && W.PartNumber(1)=="74437636352702");
near(W.L_H(1),27e-6); near(W.DCR_Ohm(1),1.48e-3);
near(W.Irms_A(1),49.3); near(W.Isat_A(1),52.1);
% Unit-free cells stay rejected for other vendors; wrong units are caught.
writecell({'Part Number','L_H','DCR (Ohm)';'si-uH-mistake',33,4.3e-3},file);
failed=false;
try
    import_loss_parts(file,"inductor");
catch e
    failed=strcmp(e.identifier,'loss:Magnitude');
end
assert(failed,'33 H must be flagged as a unit mistake.');
% Mouser export plus datasheet merge (prefix match on packaging suffix).
M=import_loss_parts(fullfile(root,'Loss_Data','Mouser_MOSFETs.csv'),"mosfet");
assert(M.PartNumber(1)=="IQE020N04LM6CGSCATMA1");
near(M.Vds_V(1),40); near(M.Rds_Ohm(1),2.05e-3); near(M.Qg_C(1),25e-9);
assert(isnan(M.Ciss_F(1)));
MF=merge_mosfet_data(M,F);
assert(height(MF)==2 && any(MF.PartNumber=="IQDH35N03LM5CGSC"));
q=MF(MF.PartNumber=="IQE020N04LM6CGSCATMA1",:);
near(q.Qg_C,33e-9); near(q.Ciss_F,2.2e-9); near(q.Qsw_C,4.9e-9);
fprintf('PASS: Wurth/Mouser import, magnitude guard, datasheet merge.\n');
fprintf('PASS: workbook import (%d inductors), SI units, buck/boost power balance,\n',height(L));
fprintf('ripple/RMS, conduction, missing data, voltage, gate timing, energy override, scope.\n');
fprintf('PASS: fixed-frequency envelope, low-voltage validity and zero-volt startup exclusion.\n');
end

function near(actual,expected)
assert(abs(actual-expected)<=max(1e-15,abs(expected)*1e-10), ...
    'Expected %.15g; got %.15g.',expected,actual);
end
