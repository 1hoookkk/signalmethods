function rows = match_body(wavPath, name, t0, t1)
Fs = 44100;
[x, fsIn] = audioread(wavPath);
x = mean(x, 2);
if fsIn ~= Fs, x = resample(x, Fs, fsIn); end
if nargin >= 4
    x = x(max(1, round(t0 * Fs)) : min(numel(x), round(t1 * Fs)));
end

f = logspace(log10(20), log10(20000), 640);
if numel(x) < 0.5 * Fs
    H = freqz(x, 1, f, Fs);
    mag = 20 * log10(abs(H) + 1e-12);
else
    [P, fw] = pwelch(x, hann(4096), 2048, 4096, Fs);
    mag = interp1(fw(2:end), 10 * log10(P(2:end) + 1e-20), f, 'linear', 'extrap');
end
mag = smoothOctave(mag, f, 1 / 6);
mag = mag - mag(find(f >= 1000, 1));

w = 2 * pi * f / Fs;
target = minimumPhase(mag, f, Fs);
wt = 1 ./ max(abs(target), 1e-3) .^ 2;
[b, a] = invfreqz(target, w, 12, 12, wt, 30);
[z, p, k] = tf2zp(b, a);
p(abs(p) > 0.999) = p(abs(p) > 0.999) ./ abs(p(abs(p) > 0.999)) * 0.999;
z(abs(z) > 1) = 1 ./ conj(z(abs(z) > 1));

poles = pairs(p, Fs);
zeros_ = pairs(z, Fs);
poles = sortrows(poles, 1);
poles = poles(1 : min(6, size(poles, 1)), :);
rows = zeros(size(poles, 1), 4);
for i = 1:size(poles, 1)
    rows(i, 1:2) = poles(i, :);
    if ~isempty(zeros_)
        [~, j] = min(abs(log2(zeros_(:, 1) / poles(i, 1))));
        rows(i, 3:4) = zeros_(j, :);
        zeros_(j, :) = [];
    else
        rows(i, 3:4) = [22050 1e9];
    end
end

body = ones(size(f));
for i = 1:size(rows, 1)
    body = body .* sectionResponse(rows(i, :), f, Fs);
end
bodyDb = 20 * log10(abs(body));
bodyDb = bodyDb - bodyDb(find(f >= 1000, 1));
band = f >= 40 & f <= 16000;
residual = max(abs(bodyDb(band) - mag(band)));

figure('Color', 'w');
semilogx(f, mag, '--', 'Color', [0.5 0.5 0.5], 'LineWidth', 1.2); hold on
semilogx(f, bodyDb, 'k', 'LineWidth', 2);
yline(0, 'k');
grid on;
xticks([20 40 80 160 320 640 1280 2560 5120 10240 20480]);
set(gca, 'XLim', [20 20000], 'YLim', [-30 30], 'YLimMode', 'manual');
xlabel('Hz'); ylabel('dB');
title(sprintf('%s  ·  %d rows  ·  max residual %.1f dB', name, size(rows, 1), residual));

disp('    pole Hz   pole BW   zero Hz   zero BW')
disp(round(rows, 2))
fprintf('max residual 40 Hz-16 kHz: %.2f dB\n', residual);
fid = fopen(fullfile(fileparts(mfilename('fullpath')), [name '.fbw']), 'w');
fprintf(fid, '# %s\n', name);
fprintf(fid, '%.4f %.4f %.4f %.4f\n', rows.');
fclose(fid);
end

function out = smoothOctave(mag, f, width)
out = mag;
for i = 1:numel(f)
    sel = f >= f(i) * 2 ^ (-width / 2) & f <= f(i) * 2 ^ (width / 2);
    out(i) = mean(mag(sel));
end
end

function H = minimumPhase(magDb, f, Fs)
N = 8192;
fl = (0:N/2) * Fs / N;
m = interp1(f, magDb, fl, 'linear', 'extrap');
m(fl < 20) = m(find(fl >= 20, 1));
logMag = [m, fliplr(m(2:end-1))] / 20 * log(10);
c = real(ifft(logMag));
c(2:N/2) = 2 * c(2:N/2);
c(N/2+2:end) = 0;
Hmp = exp(fft(c));
Hmp = Hmp(1:N/2+1);
H = interp1(fl, Hmp, f, 'linear', 'extrap');
end

function out = pairs(r, Fs)
r = r(imag(r) > 1e-6);
out = [angle(r) * Fs / (2 * pi), -log(abs(r)) * Fs / pi];
out = out(out(:, 1) > 20 & out(:, 1) < 22000, :);
end

function H = sectionResponse(row, f, Fs)
rP = exp(-pi * row(2) / Fs); thP = 2 * pi * row(1) / Fs;
rZ = exp(-pi * row(4) / Fs); thZ = 2 * pi * row(3) / Fs;
H = freqz(real(poly(rZ * exp(1j * [thZ -thZ]))), ...
          real(poly(rP * exp(1j * [thP -thP]))), f, Fs);
end
