function F = merge_mosfet_data(vendor,datasheet)
%MERGE_MOSFET_DATA Fill distributor rows (e.g. Mouser) from datasheet rows.
% Distributor exports carry only Vds/Rds/Qg, often typical values. A
% datasheet row matches when its PartNumber is a prefix of the distributor
% part (Mouser appends packaging codes, e.g. ...CGSC -> ...CGSCATMA1).
% Every finite datasheet value replaces the distributor value. Datasheet
% rows with no distributor match are kept unchanged. Unmatched distributor
% rows keep NaN dynamic data and therefore report unknown losses.
F=datasheet; used=false(height(datasheet),1);
for r=1:height(vendor)
    v=vendor(r,:);
    % startsWith(str,patterns) means "any pattern", so test each row.
    hit=find(arrayfun(@(p) startsWith(v.PartNumber,p,'IgnoreCase',true), ...
        datasheet.PartNumber));
    assert(numel(hit)<=1,'loss:MergeAmbiguous', ...
        'Several datasheet rows match %s.',v.PartNumber);
    if isempty(hit)
        F=[F;v]; %#ok<AGROW>
        continue
    end
    d=datasheet(hit,:); used(hit)=true;
    for name=["Vds_V","Rds_Ohm"]
        if isfinite(v.(name)) && abs(v.(name)-d.(name))>0.01*d.(name)
            warning('loss:MergeMismatch','%s %s: distributor %g, datasheet %g; using datasheet.', ...
                v.PartNumber,name,v.(name),d.(name));
        end
    end
    d.PartNumber=v.PartNumber;
    d.Source=v.Source+" + "+d.Source;
    F(hit,:)=d;
end
end
