clear
close all
clc

Fs = 44100;

%% THE VOWEL  (Klatt 1980 Table II: F1-F3 and B1-B3; F4 is Klatt's typical value)
vowel = 'A';
F  = [620 1220 2550 3300];
BW = [ 80   50  140  250];

%% THE HOUSE ROWS  (Talking Hedz M0 Q0, verbatim)
S1 = [10522.88 351.19 391.46 936.85];      % radiation: high pole over a low wide zero
S6 = [  225.15 124.39 7221.20 0.03];       % glottal low-pass under the cage notch

%% THE MASK RULE  (medians over the 31 factory bodies)
zeroOctavesAbove = 0.39;
zeroRadiusBelow  = 0.03;

radiusOf = @(bw) exp(-pi*bw/Fs);
bwOf     = @(r)  -log(r)*Fs/pi;

%% SIX ROWS: pole hz, pole bw, zero hz, zero bw
rows = zeros(6,4);
rows(1,:) = S1;
for k = 1:numel(F)
    rows(k+1,:) = [F(k) BW(k) F(k)*2^zeroOctavesAbove bwOf(radiusOf(BW(k)) - zeroRadiusBelow)];
end
rows(6,:) = S6;

%% ROOTS -> COEFFICIENTS, ONE BIQUAD PER ROW
f = logspace(log10(40), log10(20000), 600);
H      = ones(size(f));
Hpoles = ones(size(f));
Hrows  = zeros(6, numel(f));
for k = 1:6
    thetaP = 2*pi*rows(k,1)/Fs;  rP = radiusOf(rows(k,2));
    thetaZ = 2*pi*rows(k,3)/Fs;  rZ = radiusOf(rows(k,4));
    a = real(poly(rP * exp(1j*[thetaP -thetaP])));
    b = real(poly(rZ * exp(1j*[thetaZ -thetaZ])));
    Hk = freqz(b, a, f, Fs);
    Hrows(k,:) = Hk;
    H      = H .* Hk;
    Hpoles = Hpoles .* freqz(1, a, f, Fs);
end

%% NORMALISE AT 100 Hz
[~, i100] = min(abs(f - 100));
H      = H      / abs(H(i100));
Hpoles = Hpoles / abs(Hpoles(i100));

%% PLOT
figure('Color', 'w');
semilogx(f, 20*log10(abs(Hrows.')), 'Color', [0.75 0.75 0.75]); hold on
semilogx(f, 20*log10(abs(Hpoles)), '--', 'Color', [0.3 0.3 0.3], 'LineWidth', 1.2);
semilogx(f, 20*log10(abs(H)), 'k', 'LineWidth', 2);
grid on; xlim([40 20000]); ylim([-60 40]);
xlabel('Hz'); ylabel('dB');
title(sprintf('voice body [%s]: six rows (grey), poles alone (dashed), body (black)', vowel));

%% ROWS AND THE .fbw THE WORKSTATION OPENS
disp('    pole Hz   pole BW   zero Hz   zero BW')
disp(round(rows, 2))
name = sprintf('voice %s', vowel);
fid = fopen(fullfile(fileparts(mfilename('fullpath')), sprintf('voice_%s.fbw', vowel)), 'w');
fprintf(fid, '# %s\n', name);
fprintf(fid, '%.4f %.4f %.4f %.4f\n', rows.');
fclose(fid);
fprintf('level at 6 kHz relative to F1 peak: body %.1f dB, poles alone %.1f dB\n', ...
    20*log10(abs(H(find(f >= 6000, 1)))) - max(20*log10(abs(H(f < 1000)))), ...
    20*log10(abs(Hpoles(find(f >= 6000, 1)))) - max(20*log10(abs(Hpoles(f < 1000)))));
