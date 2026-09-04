% Z-Plane Root Extraction Script
% Extracts exact pole frequencies and radii from a single WAV file

% 1. Read Audio and Isolate a Steady Slice
[x, fs] = audioread('corner_00.wav');
% Take a steady 50ms slice of the sound and apply a Hamming window
slice_len = round(0.05 * fs);
x = x(1:slice_len) .* hamming(slice_len);

% 2. Pre-Emphasis Filtering (+6 dB/octave)
% Flattens natural acoustic radiation tilt so upper formants (F3, F4) 
% are detected by the LPC just as accurately as the heavy bass fundamentals.
preemph_coeffs = [1, -0.96]; 
x_pre = filter(preemph_coeffs, 1, x);

% 3. Burg's Method (12th-Order LPC)
% Burg's autoregressive estimation minimizes forward/backward prediction errors
% and guarantees stable poles (radius < 1.0) directly from the audio.
order = 12; 
[a, ~] = arburg(x_pre, order);

% 4. Root Extraction
rts = roots(a);
% Because coefficients are real, roots are complex conjugates. Keep only the positive half.
rts = rts(imag(rts) > 0); 

% 5. Convert Roots to ARMAdillo Hardware Coordinates
angles = angle(rts);
radii = abs(rts);

% Calculate precise Frequency (Hz) and Bandwidth (Hz)
freqs = angles * (fs / (2 * pi));
bandwidths = -(fs / pi) .* log(radii);

% Sort the roots by frequency (ascending) to maintain lane continuity
[freqs, idx] = sort(freqs);
radii = radii(idx);
bandwidths = bandwidths(idx);

% 6. Output the Coordinates for the Python Compiler
disp('--- Extracted Roots for Python Batch Compiler ---');
fprintf('Lane\t Freq (Hz)\t Radius (r)\t Bandwidth (Hz)\n');
for i = 1:length(freqs)
    fprintf('F%d\t %.2f\t\t %.4f\t\t %.2f\n', i, freqs(i), radii(i), bandwidths(i));
end