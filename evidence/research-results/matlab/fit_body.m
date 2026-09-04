function rows = fit_body(wavPath, t0, t1, name)
Fs = 44100;
[x, fsIn] = audioread(wavPath);
x = mean(x, 2);
if fsIn ~= Fs, x = resample(x, Fs, fsIn); end
seg = x(max(1, round(t0 * Fs)) : min(numel(x), round(t1 * Fs)));
seg = seg - mean(seg);
seg = seg .* hann(numel(seg));

a = arburg(seg, 12);
r = roots(a);
r = r(imag(r) > 0);
F = angle(r) * Fs / (2 * pi);
BW = -log(abs(r)) * Fs / pi;
[F, order] = sort(F);
BW = BW(order);
keep = F > 60 & F < 12000 & BW < 600;
F = F(keep);
BW = BW(keep);
n = min(4, numel(F));
F = F(1:n);
BW = BW(1:n);

S1 = [10522.88 351.19 391.46 936.85];
S6 = [225.15 124.39 7221.20 0.03];
zeroOctavesAbove = 0.39;
zeroRadiusBelow = 0.03;
radiusOf = @(bw) exp(-pi * bw / Fs);
bwOf = @(rr) -log(rr) * Fs / pi;

rows = S1;
for k = 1:n
    rows(end + 1, :) = [F(k) BW(k) F(k) * 2 ^ zeroOctavesAbove ...
        bwOf(radiusOf(BW(k)) - zeroRadiusBelow)];
end
rows(end + 1, :) = S6;

f = logspace(log10(20), log10(20000), 600);
H = ones(size(f));
for k = 1:size(rows, 1)
    rP = radiusOf(rows(k, 2)); thP = 2 * pi * rows(k, 1) / Fs;
    rZ = radiusOf(rows(k, 4)); thZ = 2 * pi * rows(k, 3) / Fs;
    H = H .* freqz(real(poly(rZ * exp(1j * [thZ -thZ]))), ...
                   real(poly(rP * exp(1j * [thP -thP]))), f, Fs);
end
E = freqz(1, a, f, Fs);
[~, i1k] = min(abs(f - 1000));
H = H / abs(H(i1k));
E = E / abs(E(i1k));

figure('Color', 'w');
semilogx(f, 20 * log10(abs(E)), '--', 'Color', [0.5 0.5 0.5], 'LineWidth', 1.2); hold on
semilogx(f, 20 * log10(abs(H)), 'k', 'LineWidth', 2);
yline(0, 'k');
grid on; xlim([20 20000]); ylim([-30 30]);
xlabel('Hz'); ylabel('dB');
title(sprintf('%s: LPC envelope (dashed), body (black)', name));

disp('    pole Hz   pole BW   zero Hz   zero BW')
disp(round(rows, 2))
fid = fopen(fullfile(fileparts(mfilename('fullpath')), [name '.fbw']), 'w');
fprintf(fid, '# %s\n', name);
fprintf(fid, '%.4f %.4f %.4f %.4f\n', rows.');
fclose(fid);
end
