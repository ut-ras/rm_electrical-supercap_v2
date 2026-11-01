function T = import_loss_parts(file,kind,sheet,unitlessSI)
%IMPORT_LOSS_PARTS Read XLSX/CSV vendor exports or canonical SI columns.
% Detects header rows (including the attached Coilcraft row-4 header).
% Common Mouser/Wurth/Coilcraft labels are recognized. Unrecognized required
% headers/units fail explicitly. Rename columns to canonical names if needed.
% Optional fields become NaN; missing data never silently becomes zero.
% unitlessSI=true treats unit-free vendor cells as SI. It is auto-enabled
% only for the Wurth catalog export (Order Code + L0 headers), whose
% values are SI (H, Ohm, A); magnitude checks below catch a wrong guess.
if nargin<3 || isempty(sheet), sheet=1; end
if nargin<4, unitlessSI=[]; end
if endsWith(lower(string(file)),[".xlsx",".xls"])
    raw=readcell(file,'Sheet',sheet);
else
    raw=readcell(file);
end
S = strings(size(raw));
for k=1:numel(raw)
    v=raw{k};
    if isempty(v) || (isnumeric(v) && all(isnan(v))), continue; end
    if ischar(v) || isstring(v) || (isnumeric(v) && isscalar(v))
        S(k)=string(v);
    end
end
S(ismissing(S))="";
partNames=["partnumber","manufacturerpartnumber","mfrpartnumber", ...
    "mfrpartno","mfrno","partno","mpn","ordercode"];
header=0;
for r=1:min(50,size(S,1))
    if any(matches(normalize(S(r,:)),partNames))
        header=r; break
    end
end
assert(header>0,'loss:Header','No part-number header found in %s.',file);
headers=S(header,:); keys=normalize(headers); data=S(header+1:end,:);
partCol=find(matches(keys,partNames),1);
if isempty(unitlessSI)
    unitlessSI=keys(partCol)=="ordercode" && any(keys=="l0");
end
keep=strlength(strtrim(data(:,partCol)))>0;
% Coilcraft appends a labeled results-page hyperlink after the part table.
footer=startsWith(lower(strtrim(data(:,partCol))),"link to results page");
keep=keep & ~footer;
data=data(keep,:);
T=table(strtrim(data(:,partCol)),'VariableNames',{'PartNumber'});
% canonical SI name | vendor header regex | physical unit | required
if kind=="inductor"
    spec={
        'L_H','^(lnominal|inductance)|^l(uh|mh|h|0)$','H',true;
        'DCR_Ohm','^(dcr|rdc|dcresistance|maximumdcresistance)','Ohm',true;
        'Isat_A','^(isat|saturationcurrent)','A',false;
        'Irms_A','^(irms|currentrating)|^ir$','A',false;
        'CoreLoss_W','^coreloss','W',false;
        'ACWindingLoss_W','^acwindingloss','W',false};
elseif kind=="mosfet"
    spec={
        'Vds_V','^(vdss|vds|drainsourcebreakdownvoltage)','V',true;
        'Rds_Ohm','^(rdson|rds|drainsourceonresistance)','Ohm',true;
        'Qg_C','^(qg|gatechargetotal|totalgatecharge)','C',false;
        'Qgd_C','^(qgd|gatetodraincharge)','C',false;
        'Qsw_C','^(qsw|switchingcharge)','C',false;
        'Vplateau_V','^(vplateau|gateplateauvoltage)','V',false;
        'Ciss_F','^(ciss|inputcapacitance)','F',false;
        'Coss_F','^(coss|outputcapacitance)','F',false;
        'Qoss_C','^(qoss|outputcharge)','C',false;
        'Ecomm_J','^ecomm','J',false;
        'Eon_J','^eon','J',false;
        'Eoff_J','^eoff','J',false;
        'Qrr_C','^(qrr|reverserecoverycharge)','C',false;
        'Vf_V','^(vf|diodeforwardvoltage)','V',false;
        'RgInternal_Ohm','^(rginternal|gateresistance)','Ohm',false;
        'RdsVgs_V','^rdsvgs','V',false;
        'QgVgs_V','^qgvgs','V',false;
        'DataVds_V','^datavds','V',false};
else
    error('loss:Kind','Kind must be inductor or mosfet.');
end
for k=1:size(spec,1)
    name=spec{k,1}; pattern=spec{k,2}; unit=spec{k,3}; required=spec{k,4};
    col=find(keys==normalize(string(name)));
    canonical=~isempty(col);
    if ~canonical
        col=find(~cellfun(@isempty,regexp(cellstr(keys),pattern,'once')));
        % Prevent Qg from accidentally matching Qgd, QgVgs, etc.
        if strcmp(name,'Qg_C')
            col=col(~startsWith(keys(col),["qgd","qgvgs","qgth"]));
        elseif strcmp(name,'Rds_Ohm')
            col=col(~startsWith(keys(col),"rdsvgs"));
        end
    end
    if isempty(col)
        assert(~required,'loss:Column','Missing %s in %s.',name,file);
        T.(name)=nan(height(T),1); continue
    end
    assert(isscalar(col),'loss:Ambiguous','Multiple columns match %s; use canonical names.',name);
    values=nan(height(T),1);
    for r=1:height(T)
        values(r)=quantity(data(r,col),headers(col),unit,canonical || unitlessSI);
    end
    if required
        assert(all(isfinite(values) & values>0),'loss:Value', ...
            '%s must be positive numeric values in every part row.',name);
    else
        assert(all(isnan(values) | (isfinite(values) & values>=0)), ...
            'loss:Value','Invalid values in %s.',name);
    end
    T.(name)=values;
end
% Coarse plausibility limits catch unit mistakes (e.g. uH read as H).
limits={'L_H',1e-9,1e-1;'DCR_Ohm',1e-5,10;'Rds_Ohm',1e-5,10;'Vds_V',1,2000};
for j=1:size(limits,1)
    if any(strcmp(T.Properties.VariableNames,limits{j,1}))
        v=T.(limits{j,1});
        assert(all(isnan(v) | (v>=limits{j,2} & v<=limits{j,3})),'loss:Magnitude', ...
            '%s in %s is outside %g..%g; check units.',limits{j,1},file,limits{j,2},limits{j,3});
    end
end
[~,name,ext]=fileparts(string(file));
T.Source=repmat(name+ext,height(T),1);
end

function out=normalize(s)
s=lower(replace(string(s),["µ","μ","Ω","Ω"],["u","u","ohm","ohm"]));
out=regexprep(s,'[^a-z0-9]','');
end

function v=quantity(s,header,baseUnit,canonical)
s=strtrim(s);
if ismissing(s) || any(strcmpi(s,["","-","--","n/a","na","nan"]))
    v=NaN; return
end
% Only one numeric quantity; reject ranges, test-condition strings and +/-.
tok=regexp(char(s),'^([+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][+-]?\d+)?)\s*([^\d]*)$','tokens','once');
assert(~isempty(tok),'loss:Quantity','Cannot parse "%s" under "%s". Use a single value and unit.',s,header);
v=str2double(tok{1}); suffix=strtrim(string(tok{2}));
if strlength(suffix)>0
    factor=unitFactor(suffix,baseUnit);
    assert(isfinite(factor),'loss:Unit','Unknown unit in "%s".',s);
elseif canonical
    factor=1;
else
    % Header may contain ratings/test conditions; examine parenthesized unit
    % or last standalone unit token, never infer micro/milli from magnitude.
    h=replace(string(header),["µ","μ","Ω","Ω"],["u","u","Ohm","Ohm"]);
    tokens=regexp(char(h),'[A-Za-z]+','match'); factor=NaN;
    for j=1:numel(tokens)
        candidate=unitFactor(string(tokens{j}),baseUnit);
        if isfinite(candidate), factor=candidate; end
    end
    assert(isfinite(factor),'loss:Unit', ...
        'No recognized unit in header "%s"; rename to an SI column name.',header);
end
v=v*factor;
end

function f=unitFactor(s,u)
s=replace(strtrim(s),["µ","μ","Ω","Ω"],["u","u","Ohm","Ohm"]);
s=regexprep(s,'[()]','');
if strcmp(u,'Ohm')
    s=regexprep(s,'(?i)ohms?$','Ohm');
end
if strcmp(s,u), f=1; return; end
prefix=["p","n","u","m","k","M"];
scale=[1e-12,1e-9,1e-6,1e-3,1e3,1e6];
j=find(s==prefix+string(u),1);
if isempty(j), f=NaN; else, f=scale(j); end
end
