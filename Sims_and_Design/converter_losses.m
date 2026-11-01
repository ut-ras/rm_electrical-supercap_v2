function R = converter_losses(Ls,Fs,c)
%CONVERTER_LOSSES One PWM half bridge, identical MOSFETs, positive CCM current.
% Supports buck charge and reversed boost discharge, Vcap < Vbus. IL is the
% low-voltage-port current. No simultaneous two-leg PWM or DCM model.
% ModelledTotal stays NaN if any loss term is unknown. KnownLossSubtotal is
% useful for screening only and is not necessarily a rigorous lower bound.
validateConfig(c);
assert(height(Ls)>0 && height(Fs)>0,'loss:Empty','No component rows.');
V=c.Vcap_V(:); I=c.IL_A(:);
if isscalar(I), I=repmat(I,numel(V),1); end
assert(numel(I)==numel(V),'loss:OperatingPoints','IL must be scalar or match Vcap.');
rows=cell(height(Ls)*height(Fs)*numel(V),1); k=0;
for op=1:numel(V)
    duty=V(op)/c.Vbus_V;
    capR=c.capEachESR_Ohm*c.capSeries/c.capParallel;
    for a=1:height(Ls)
        l=Ls(a,:); Leff=l.L_H*c.Lfraction;
        requiredF=V.*(c.Vbus_V-V)./(c.Vbus_V*c.rippleRatio*I*Leff);
        if c.frequencyMode=="envelope"
            f=max(requiredF);
        else
            f=requiredF(op);
        end
        f=max(f,c.fswFloor_Hz); % e.g. stay above audible range; ripple drops
        ripple=V(op)*(c.Vbus_V-V(op))/(c.Vbus_V*Leff*f);
        i2=I(op)^2+ripple^2/12;
        imin=I(op)-ripple/2; imax=I(op)+ripple/2;
        busCapI2=I(op)^2*duty*(1-duty)+duty*ripple^2/12;
        pulse=min(duty,1-duty)/f;
        for b=1:height(Fs)
            m=Fs(b,:); k=k+1; x=struct;
            x.Inductor=l.PartNumber; x.MOSFET=m.PartNumber;
            x.Mode=c.mode; x.Vcap_V=V(op); x.IL_A=I(op);
            x.Leffective_H=Leff; x.Fsw_Hz=f; x.DutyHighSide=duty;
            x.RequiredFswAtPoint_Hz=requiredF(op); x.FrequencyMode=c.frequencyMode;
            x.ActualRippleRatio=ripple/I(op);
            x.DeltaIpp_A=ripple; x.Ipeak_A=imax; x.Irms_A=sqrt(i2);
            x.CapBank_F=c.capEach_F*c.capParallel/c.capSeries;
            x.CapBankESR_Ohm=capR; x.CapEachIrms_A=sqrt(i2)/c.capParallel;
            x.BusBufferIrms_A=sqrt(busCapI2);
            x.PinductorDC_W=i2*l.DCR_Ohm*(1+c.copperAlpha_perC*(c.inductorTemp_C-25));
            x.Pcore_W=l.CoreLoss_W; x.PwindingAC_W=l.ACWindingLoss_W;
            x.Psupercap_W=i2*capR;
            x.PbusBuffer_W=busCapI2*c.bufferESR_Ohm;
            rd=m.Rds_Ohm*c.rdsTemperatureFactor;
            converterR=x.PinductorDC_W/i2+(1+c.extraSeriesFETs)*rd;
            x.ConductionDropFraction=I(op)*converterR/V(op);
            if c.mode=="buck_charge"
                x.CapInternalMean_V=V(op)-I(op)*capR;
            else
                x.CapInternalMean_V=V(op)+I(op)*capR;
            end
            approxOK=double(x.ConductionDropFraction<=c.maxConductionDropFraction && ...
                x.CapInternalMean_V>=0);
            x.PfetHighConduction_W=duty*i2*rd;
            x.PfetLowConduction_W=(1-duty)*i2*rd;
            x.PfetHeldConduction_W=c.extraSeriesFETs*i2*rd;
            x.PfetConduction_W=x.PfetHighConduction_W+x.PfetLowConduction_W+x.PfetHeldConduction_W;
            % Ideal-duty conduction slightly overlaps the small deadtime
            % interval; retained as a conservative first-order approximation.
            x.Pdeadtime_W=2*c.deadtime_s*f*I(op)*m.Vf_V;
            x.PgateSupply_W=2*m.Qg_C*c.Vdrive_V*f; % two toggling gates
            x.PdriverOverhead_W=c.driverOverhead_W;
            rsource=c.Rsource_Ohm+c.RgateExternal_Ohm+m.RgInternal_Ohm;
            rsink=c.Rsink_Ohm+c.RgateExternal_Ohm+m.RgInternal_Ohm;
            if isfinite(m.Vplateau_V) && m.Vplateau_V>0 && m.Vplateau_V<c.Vdrive_V && ...
                    isfinite(rsource) && isfinite(rsink)
                ion=min(c.IpeakSource_A,(c.Vdrive_V-m.Vplateau_V)/rsource);
                ioff=min(c.IpeakSink_A,m.Vplateau_V/rsink);
            else
                ion=NaN; ioff=NaN;
            end
            x.GatePlateauSource_A=ion; x.GatePlateauSink_A=ioff;
            qswitch=m.Qsw_C;
            if ~isfinite(qswitch), qswitch=m.Qgd_C; end
            x.OverlapOn_s=qswitch/ion; x.OverlapOff_s=qswitch/ioff;
            % Qsw includes the current-rise contribution when available.
            % Qgd-only result omits it and is labeled accordingly below.
            if c.mode=="buck_charge"
                ionLoad=imin; ioffLoad=imax;
            else
                ionLoad=imax; ioffLoad=imin;
            end
            x.Poverlap_W=0.5*c.Vbus_V*f*(ionLoad*x.OverlapOn_s+ioffLoad*x.OverlapOff_s);
            x.PreverseRecovery_W=c.Vbus_V*m.Qrr_C*f;
            if isfinite(m.Ecomm_J)
                x.PcapSwitching_W=m.Ecomm_J*f;
                capMethod="provided half-bridge commutation energy";
            elseif isfinite(m.Qoss_C)
                x.PcapSwitching_W=c.Vbus_V*m.Qoss_C*f;
                capMethod="identical-pair Vbus*Qoss estimate";
            else
                x.PcapSwitching_W=m.Coss_F*c.Vbus_V^2*f;
                capMethod="constant-Coss pair estimate";
            end
            % Eon/Eoff must cover the WHOLE switching leg at this operating
            % point, including Coss and reverse recovery. Never add twice.
            measured=isfinite(m.Eon_J) && isfinite(m.Eoff_J);
            if measured
                x.Pswitching_W=(m.Eon_J+m.Eoff_J)*f;
                method="provided whole-leg Eon+Eoff; component estimates excluded";
            else
                x.Pswitching_W=x.Poverlap_W+x.PreverseRecovery_W+x.PcapSwitching_W;
                method=capMethod;
            end
            x.SwitchingMethod=method;
            x.Pconduction_W=x.PinductorDC_W+x.Psupercap_W+x.PfetConduction_W;
            % Qg/current and RC settling are independent coarse timing screens.
            % Test both against shortest PWM interval, including delay/deadtime.
            x.GateChargeTime_s=max(m.Qg_C/ion,m.Qg_C/ioff);
            x.GateRC10to90_s=log(9)*m.Ciss_F*max(rsource,rsink);
            available=pulse-c.propagation_s-c.deadtime_s;
            x.ShortestPulse_s=pulse;
            x.CissLimit_F=c.maxGatePulseFraction*available/(log(9)*max(rsource,rsink));
            gateCheck=max(x.GateChargeTime_s,x.GateRC10to90_s);
            if ~all(isfinite([m.Qg_C,m.Ciss_F,ion,ioff,gateCheck]))
                gateOK=NaN;
            else
                gateOK=double(available>0 && gateCheck<=c.maxGatePulseFraction*available);
            end
            checks=[double(f>=c.minFsw_Hz && f<=c.maxFsw_Hz), ...
                compare(l.Isat_A,imax),compare(l.Irms_A,sqrt(i2)), ...
                compare(m.Vds_V,(c.VbusMax_V+c.overshoot_V)*c.voltageRatingFactor), ...
                gateOK,double(pulse>=c.minPulse_s && available>0), ...
                compare(c.capEachRated_V,max(V(op),x.CapInternalMean_V)/c.capSeries), ...
                compare(c.capEachIrms_A,x.CapEachIrms_A),approxOK];
            labels=["frequency","inductor saturation","inductor RMS", ...
                "MOSFET voltage","gate timing","PWM pulse", ...
                "capacitor voltage","capacitor RMS","ideal-duty approximation"];
            x.FrequencyOK=checks(1); x.IsatOK=checks(2); x.InductorRmsOK=checks(3);
            x.VoltageOK=checks(4); x.GateTimingOK=checks(5);
            x.CapVoltageOK=checks(7); x.CapRmsOK=checks(8);
            x.IdealDutyApproximationOK=checks(9);
            x.ScreenFailures=sum(checks==0); x.ScreenUnknowns=sum(isnan(checks));
            x.Screen="SCREEN_ONLY";
            if x.ScreenUnknowns>0, x.Screen="INCOMPLETE"; end
            if x.ScreenFailures>0, x.Screen="FAIL"; end
            terms=[x.PinductorDC_W,x.Pcore_W,x.PwindingAC_W,x.Psupercap_W, ...
                x.PbusBuffer_W,x.PfetConduction_W,x.Pdeadtime_W, ...
                x.PgateSupply_W,x.PdriverOverhead_W];
            if measured
                terms=[terms,x.Pswitching_W]; %#ok<AGROW>
            else
                terms=[terms,x.Poverlap_W,x.PreverseRecovery_W,x.PcapSwitching_W]; %#ok<AGROW>
            end
            x.KnownLossSubtotal_W=sum(terms(isfinite(terms)));
            x.UnknownLossTerms=sum(~isfinite(terms));
            x.ModelledTotal_W=sum(terms);
            x.CapTerminalPower_W=V(op)*I(op);
            % Vcap is the terminal voltage. ESR heating is internal to the
            % bank; do not count it twice in the converter input-power budget.
            if c.mode=="buck_charge"
                x.StoredPower_W=V(op)*I(op)-x.Psupercap_W;
                x.RequiredBusPower_W=V(op)*I(op)+x.ModelledTotal_W-x.Psupercap_W;
                x.DeliveredBusPower_W=NaN;
                x.Efficiency_pct=100*x.StoredPower_W/x.RequiredBusPower_W;
                x.BudgetKnownExceeded=double(V(op)*I(op)+x.KnownLossSubtotal_W-x.Psupercap_W>c.powerBudget_W);
            else
                x.StoredPower_W=-(V(op)*I(op)+x.Psupercap_W);
                x.RequiredBusPower_W=NaN;
                x.DeliveredBusPower_W=V(op)*I(op)-(x.ModelledTotal_W-x.Psupercap_W);
                x.Efficiency_pct=100*x.DeliveredBusPower_W/(-x.StoredPower_W);
                x.BudgetKnownExceeded=NaN; % charging budget does not limit peak discharge
            end
            note="Nominal L/ratings need bias and thermal checks; switching estimates need bench validation. ";
            if any(checks==0), note=note+"FAIL: "+strjoin(labels(checks==0),", ")+". "; end
            if any(isnan(checks)), note=note+"UNKNOWN: "+strjoin(labels(isnan(checks)),", ")+". "; end
            if ~isfinite(m.Qsw_C), note=note+"Qgd-only overlap excludes current-rise charge. "; end
            if ~isfinite(m.DataVds_V) || abs(m.DataVds_V-c.Vbus_V)>1e-6
                note=note+"Dynamic data Vds differs or is unknown. ";
            end
            if ~isfinite(m.RdsVgs_V) || ~isfinite(m.QgVgs_V) || ...
                    abs(m.RdsVgs_V-c.Vdrive_V)>1e-6 || abs(m.QgVgs_V-c.Vdrive_V)>1e-6
                note=note+"Rds/Qg gate-voltage conditions differ or are unknown. ";
            end
            if x.UnknownLossTerms>0, note=note+"Unknown loss terms omitted from subtotal. "; end
            if x.BudgetKnownExceeded==1, note=note+"Charging exceeds 90W/configured budget from known terms. "; end
            x.Notes=note;
            rows{k}=x;
        end
    end
end
R=struct2table(vertcat(rows{:}));
end

function y=compare(rating,required)
if ~isfinite(rating), y=NaN; else, y=double(rating>=required); end
end

function validateConfig(c)
assert(any(c.mode==["buck_charge","boost_discharge"]),'loss:Mode','Unsupported mode.');
assert(any(c.frequencyMode==["per_point","envelope"]),'loss:FrequencyMode','Unsupported frequency mode.');
assert(all(c.Vcap_V(:)>0),'loss:Startup', ...
    'Vcap=0 is outside steady-state CCM: use a current-limited startup model; boost cannot supply power from 0 V.');
positive={'Vbus_V','IL_A','Vcap_V','rippleRatio','VbusMax_V', ...
    'voltageRatingFactor','rdsTemperatureFactor','Lfraction','minFsw_Hz', ...
    'maxFsw_Hz','capEach_F','Vdrive_V','Rsource_Ohm','Rsink_Ohm', ...
    'IpeakSource_A','IpeakSink_A','maxGatePulseFraction','powerBudget_W', ...
    'maxConductionDropFraction'};
for j=1:numel(positive)
    validateattributes(c.(positive{j}),{'numeric'},{'positive','finite','nonempty'});
end
assert(all(c.Vcap_V<c.Vbus_V) && c.rippleRatio<2,'loss:CCM','Requires 0<Vcap<Vbus and rippleRatio<2.');
assert(c.Lfraction<=1 && c.maxGatePulseFraction<=1 && c.voltageRatingFactor>=1);
assert(isscalar(c.maxConductionDropFraction) && c.maxConductionDropFraction<=1);
assert(c.minFsw_Hz<c.maxFsw_Hz && c.VbusMax_V>=c.Vbus_V);
assert(c.extraSeriesFETs==0 || c.extraSeriesFETs==1);
for name={'capSeries','capParallel'}
    validateattributes(c.(name{1}),{'numeric'},{'scalar','integer','positive'});
end
for name={'deadtime_s','propagation_s','minPulse_s','RgateExternal_Ohm', ...
        'capEachESR_Ohm','overshoot_V','copperAlpha_perC','fswFloor_Hz'}
    validateattributes(c.(name{1}),{'numeric'},{'scalar','nonnegative','finite'});
end
for name={'bufferESR_Ohm','driverOverhead_W','capEachRated_V','capEachIrms_A'}
    v=c.(name{1}); assert(isscalar(v) && (isnan(v) || (isfinite(v) && v>=0)));
end
validateattributes(c.inductorTemp_C,{'numeric'},{'scalar','finite','>=',-50,'<=',200});
end
