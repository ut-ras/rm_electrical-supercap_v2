w = logspace(0, 10, 1000);   % adjust range if needed
s = 1j * w;                 % s = jω

G = (0.0133*s + 1) ./ (2.97e-10*s.^2 + 0.0133*s + 1);

mag = 20*log10(abs(G));
phase = angle(G) * (180/pi);

subplot(2,1,1)
semilogx(w, mag)
ylabel('Magnitude (dB)')
grid on

subplot(2,1,2)
semilogx(w, phase)
ylabel('Phase (deg)')
xlabel('Frequency (rad/s)')
grid on