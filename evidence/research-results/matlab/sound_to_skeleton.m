clear;
close all;
clc;

%% ============================================================
%  SOUND -> LPC POLE SKELETON
%
%  Output:
%    - up to 6 complex pole pairs
%    - frequency in Hz
%    - radius
%    - bandwidth in Hz
%    - spectrum vs LPC envelope
%    - CSV export
%
%  Requires: Signal Processing Toolbox
%% ============================================================

%% SETTINGS

lpcOrder = 12;          % 12 poles = max 6 complex pole pairs
frame_ms = 120;         % analysis window length
manualStartSec = [];    % [] = automatically choose loud stable-ish region
                        % e.g. 1.25 = start at 1.25 seconds

minFreqHz = 40;
maxFreqHz = 18000;

%% LOAD WAV

[file, path] = uigetfile( ...
    {'*.wav;*.flac;*.aiff;*.aif','Audio files'}, ...
    'Choose a sound');

if isequal(file,0)
    error('No audio file selected.');
end

filename = fullfile(path,file);

[x, Fs] = audioread(filename);

fprintf('\nLoaded: %s\n', file);
fprintf('Sample rate: %d Hz\n', Fs);
fprintf('Length: %.2f seconds\n\n', length(x)/Fs);

%% MONO

if size(x,2) > 1
    x = mean(x,2);
end

x = x - mean(x);

peak = max(abs(x));

if peak > 0
    x = x / peak;
end

%% CHOOSE ANALYSIS FRAME

frameSamples = round(frame_ms / 1000 * Fs);

if frameSamples >= length(x)
    frameSamples = length(x);
end

if isempty(manualStartSec)

    % Find the frame with the highest RMS energy.
    % 25% overlap between candidate positions.

    hop = max(1, round(frameSamples/4));

    starts = 1:hop:(length(x)-frameSamples+1);

    rmsValues = zeros(size(starts));

    for k = 1:length(starts)
        seg = x(starts(k):starts(k)+frameSamples-1);
        rmsValues(k) = sqrt(mean(seg.^2));
    end

    [~,best] = max(rmsValues);

    startSample = starts(best);

else

    startSample = round(manualStartSec * Fs) + 1;

    if startSample < 1
        startSample = 1;
    end

    if startSample + frameSamples - 1 > length(x)
        startSample = length(x) - frameSamples + 1;
    end

end

stopSample = startSample + frameSamples - 1;

frame = x(startSample:stopSample);

startSec = (startSample-1)/Fs;
stopSec  = (stopSample-1)/Fs;

fprintf('Analysis region: %.3f - %.3f sec\n', startSec, stopSec);
fprintf('Frame length: %.1f ms\n\n', 1000*length(frame)/Fs);

%% WINDOW THE FRAME

frame = frame - mean(frame);

w = hann(length(frame),'periodic');

frameWindowed = frame .* w;

%% LPC

[a, predictionError] = lpc(frameWindowed, lpcOrder);

allPoles = roots(a);

%% KEEP ONE ROOT FROM EACH COMPLEX CONJUGATE PAIR

poles = allPoles(imag(allPoles) > 1e-8);

poleFreqHz = angle(poles) * Fs / (2*pi);
poleRadius = abs(poles);

% Pole bandwidth:
%
%       BW = -(Fs/pi) ln(r)

poleBWHz = -(Fs/pi) .* log(poleRadius);

%% REMOVE POLES OUTSIDE USEFUL AUDIO RANGE

upperLimit = min(maxFreqHz, Fs/2 * 0.98);

keep = ...
    poleFreqHz >= minFreqHz & ...
    poleFreqHz <= upperLimit & ...
    poleRadius > 0 & ...
    poleRadius < 1;

poles      = poles(keep);
poleFreqHz = poleFreqHz(keep);
poleRadius = poleRadius(keep);
poleBWHz   = poleBWHz(keep);

%% SORT LOW -> HIGH FREQUENCY

[poleFreqHz, idx] = sort(poleFreqHz);

poleRadius = poleRadius(idx);
poleBWHz   = poleBWHz(idx);
poles      = poles(idx);

%% DISPLAY RESULTS

fprintf('====================================================\n');
fprintf('LPC POLE SKELETON\n');
fprintf('====================================================\n');
fprintf(' #        Hz       radius       BW Hz\n');
fprintf('----------------------------------------------------\n');

for k = 1:length(poleFreqHz)

    fprintf('%2d   %8.1f     %.6f     %8.1f\n', ...
        k, ...
        poleFreqHz(k), ...
        poleRadius(k), ...
        poleBWHz(k));

end

fprintf('====================================================\n');

fprintf('\nComplex pole pairs found: %d\n', length(poleFreqHz));

if length(poleFreqHz) < 6
    fprintf(['NOTE: order-12 LPC did not produce six usable complex ' ...
             'pole pairs.\n']);
    fprintf('Some LPC roots may be real. Nothing has been invented.\n');
end

%% ACTUAL AUDIO SPECTRUM

nfft = 16384;

X = fft(frameWindowed,nfft);

fFFT = (0:nfft/2)' * Fs/nfft;

magFFT = 20*log10(abs(X(1:nfft/2+1)) + 1e-12);

%% LPC ENVELOPE

[HLPC,fLPC] = freqz(sqrt(predictionError),a,nfft/2+1,Fs);

magLPC = 20*log10(abs(HLPC) + 1e-12);

%% NORMALISE FOR VISUAL COMPARISON

plotRangeFFT = ...
    fFFT >= minFreqHz & ...
    fFFT <= upperLimit;

plotRangeLPC = ...
    fLPC >= minFreqHz & ...
    fLPC <= upperLimit;

magFFT = magFFT - max(magFFT(plotRangeFFT));
magLPC = magLPC - max(magLPC(plotRangeLPC));

%% PLOT 1 — WAVEFORM + CHOSEN FRAME

t = (0:length(x)-1)/Fs;

figure('Name','Selected analysis region');

plot(t,x);
hold on;

xline(startSec,'--');
xline(stopSec,'--');

xlabel('Time (seconds)');
ylabel('Amplitude');
title('Selected LPC analysis region');

grid on;

%% PLOT 2 — SOUND SPECTRUM VS LPC ENVELOPE

figure('Name','LPC skeleton');

semilogx(fFFT,magFFT,'LineWidth',0.8);
hold on;

semilogx(fLPC,magLPC,'LineWidth',2);

for k = 1:length(poleFreqHz)
    xline(poleFreqHz(k),':');
end

grid on;

xlim([minFreqHz upperLimit]);
ylim([-60 10]);

xlabel('Frequency (Hz)');
ylabel('Relative magnitude (dB)');
title(sprintf('LPC order %d — %s',lpcOrder,file), ...
      'Interpreter','none');

legend('Audio spectrum','LPC envelope','Pole frequencies');

%% PLOT 3 — POLES

figure('Name','LPC pole map');

zplane([],a);

title('LPC poles');

%% EXPORT TABLE

Skeleton = table( ...
    (1:length(poleFreqHz))', ...
    poleFreqHz, ...
    poleRadius, ...
    poleBWHz, ...
    'VariableNames', ...
    {'Pole','Frequency_Hz','Radius','Bandwidth_Hz'});

disp(Skeleton);

[outPath,outName,~] = fileparts(filename);

csvFile = fullfile(outPath,[outName '_lpc_skeleton.csv']);

writetable(Skeleton,csvFile);

fprintf('\nSaved:\n%s\n',csvFile);

%% OPTIONAL .FBW EXPORT
%
% One line per pole:
%
% frequency_hz bandwidth_hz

fbwFile = fullfile(outPath,[outName '_lpc_skeleton.fbw']);

fid = fopen(fbwFile,'w');

for k = 1:length(poleFreqHz)
    fprintf(fid,'%.6f %.6f\n', ...
        poleFreqHz(k), ...
        poleBWHz(k));
end

fclose(fid);

fprintf('%s\n',fbwFile);