import torch


class AudioObjective(torch.nn.Module):
    def __init__(self, sample_rate=48000, fft_sizes=(256, 1024, 4096), low_frequency_hz=300.0, spectral_weight=1.0, low_frequency_weight=1.0, denominator_floor_db=-60.0, waveform_weight=0.0):
        super().__init__()
        self.waveform_weight = waveform_weight
        self.sample_rate = sample_rate
        self.fft_sizes = fft_sizes
        self.low_frequency_hz = low_frequency_hz
        self.spectral_weight = spectral_weight
        self.low_frequency_weight = low_frequency_weight
        self.denominator_floor_db = denominator_floor_db
        self.rms_floor = 10 ** (denominator_floor_db / 20)
        for size in fft_sizes:
            self.register_buffer(f'window_{size}', torch.hann_window(size))

    def forward(self, prediction, target):
        if prediction.ndim == 1:
            prediction, target = prediction.unsqueeze(0), target.unsqueeze(0)
        resolutions = []
        for size in self.fft_sizes:
            window = getattr(self, f'window_{size}')
            p = torch.stft(prediction, size, size // 4, window=window, return_complex=True).abs()
            t = torch.stft(target, size, size // 4, window=window, return_complex=True).abs()
            norm_floor = self.rms_floor * (size * 0.5 * window.square().sum() * t.shape[-1]).sqrt()
            convergence = (torch.linalg.vector_norm(p-t, dim=(-2, -1)) / torch.linalg.vector_norm(t, dim=(-2, -1)).clamp_min(norm_floor)).mean()
            log_magnitude = (torch.log(p.clamp_min(1e-5)) - torch.log(t.clamp_min(1e-5))).abs().mean()
            resolutions.append(convergence + log_magnitude)
        spectral = torch.stack(resolutions).mean()
        length = target.shape[-1]
        window = torch.hann_window(length, device=target.device, dtype=target.dtype)
        scale = window.square().sum().sqrt()
        p = torch.fft.rfft(prediction * window, dim=-1) / scale
        t = torch.fft.rfft(target * window, dim=-1) / scale
        cutoff_bin = min(int(self.low_frequency_hz * length / self.sample_rate) + 1, t.shape[-1])
        weights = torch.full((cutoff_bin,), 2.0, device=t.device, dtype=target.dtype)
        weights[0] = 1
        if length % 2 == 0 and cutoff_bin == t.shape[-1]:
            weights[-1] = 1
        error = ((p[:, :cutoff_bin] - t[:, :cutoff_bin]).abs().square() * weights).sum(-1) / length
        power = (t[:, :cutoff_bin].abs().square() * weights).sum(-1) / length
        low_frequency = (error / power.clamp_min(self.rms_floor ** 2)).mean()
        waveform = ((prediction - target).square().sum(-1) / target.square().sum(-1).clamp_min(self.rms_floor ** 2 * length)).mean()
        total = self.spectral_weight * spectral + self.low_frequency_weight * low_frequency + self.waveform_weight * waveform
        return total, {'mr_spectral': spectral, 'lf_esr': low_frequency, 'waveform_esr': waveform}

    @torch.no_grad()
    def evaluate(self, prediction, target, block=4096):
        prediction, target = prediction.flatten(), target.flatten()
        totals = torch.zeros(4, device=prediction.device)
        length = len(target)
        if length == 0 or prediction.shape != target.shape:
            raise ValueError('Evaluation needs equal, nonempty signals')
        for start in range(0, length, block * 16):
            p, t = prediction[start:start+block*16], target[start:start+block*16]
            valid = len(t)
            padding = (-valid) % block
            p = torch.nn.functional.pad(p, (0, padding)).reshape(-1, block)
            t = torch.nn.functional.pad(t, (0, padding)).reshape(-1, block)
            loss, parts = self(p, t)
            totals += torch.stack((loss, parts['mr_spectral'], parts['lf_esr'], parts['waveform_esr'])) * valid
        totals /= length
        return totals[0], {'mr_spectral': totals[1], 'lf_esr': totals[2], 'waveform_esr': totals[3]}

    def description(self):
        return {'name': 'MR spectral loss + low-frequency ESR' + (' + full-band waveform ESR' if self.waveform_weight > 0 else ''), 'waveform_weight': self.waveform_weight, 'fft_sizes': list(self.fft_sizes), 'hop_fraction': 0.25, 'block_samples': 4096, 'spectral_terms': ['spectral convergence', 'log magnitude L1'], 'low_frequency_band_hz': [0, self.low_frequency_hz], 'low_frequency_method': 'Complex FFT error power / target power on Hann-windowed blocks, including DC, with one-sided Parseval weights', 'denominator_rms_floor_dbfs': self.denominator_floor_db, 'spectral_weight': self.spectral_weight, 'low_frequency_weight': self.low_frequency_weight, 'full_band_sample_error_in_objective': self.waveform_weight > 0, 'normalization_or_gain_matching': False}
