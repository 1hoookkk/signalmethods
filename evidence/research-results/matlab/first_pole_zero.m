clear
close all
clc

Fs = 44100;

%% ONE POLE PAIR
poleFreq = 1000;
poleRadius = 0.97;

thetaP = 2*pi*poleFreq/Fs;

poles = poleRadius * exp(1j*[thetaP -thetaP]);

%% ONE ZERO PAIR
zeroFreq = 2500;
zeroRadius = 0.94;

thetaZ = 2*pi*zeroFreq/Fs;

zeros_ = zeroRadius * exp(1j*[thetaZ -thetaZ]);

%% TURN ROOTS INTO FILTER COEFFICIENTS
a = real(poly(poles));
b = real(poly(zeros_));

%% NORMALISE AT 100 Hz
Htmp = freqz(b,a,2*pi*[100 101]/Fs);
H100 = Htmp(1);
b = b / abs(H100);

%% FREQUENCY RESPONSE
[H,f] = freqz(b,a,8192,Fs);

mag = 20*log10(abs(H));

figure
semilogx(f,mag,'LineWidth',1.5)
grid on
xlim([20 20000])
ylim([-30 30])
xlabel('Frequency (Hz)')
ylabel('Magnitude (dB)')
title('One pole pair + one zero pair')

%% POLE/ZERO MAP
figure
zplane(b,a)
title('Poles and zeros')