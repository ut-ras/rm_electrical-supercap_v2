FileID = readtable("circuit_data.csv");
%%
figure;
hold on;
plot(FileID.Time, FileID.Ibat);
plot(FileID.Time, FileID.Iref);
legend(["Battery Current" "Reference Current"]);