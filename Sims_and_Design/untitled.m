
f0 = 440;
fs = 8000;
Ts = 1 / fs;
t = 0 : Ts : 5;
x = cos(2*pi*f0*t);

y1 = 0.5+0.5*cos(2*pi*880*t);
figure;
spectrogram(y, hamming(256), 128, 256, fs, 'yaxis'); 
a1 = y1 - mean(y1);
soundsc(a1, fs);
