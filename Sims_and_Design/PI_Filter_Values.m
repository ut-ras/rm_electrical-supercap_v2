w_n = 2*pi*12;
L = 27e-6;

C_values = linspace(10e-6,10e-5, 10000);
R_values = linspace(10e2, 10e4, 100000);

C_list = [];
R_list = [];
a_list = [];

for C = C_values
    invLC = L * C;   % precompute scaling factor

    for R = R_values
        a = R/L - w_n;

        ratio = (w_n * a) * invLC;

        if abs(ratio - 1) < 1e-5
            C_list(end+1) = C;
            R_list(end+1) = R;
            a_list(end+1) = a;
        end
    end
end

C_list_uF = C_list * 1e6;
R_list_kOhm = R_list * 1e-3;

T = table(C_list_uF', R_list_kOhm', a_list', ...
    'VariableNames', {'C_uF', 'R_kOhm', 'a'});

writetable(T, 'pi_filter_solutions.xlsx', 'WriteMode', 'overwrite');

disp(['Exported ' num2str(height(T)) ' valid points to pi_filter_solutions.xlsx']);