import os, csv, math, json
from collections import defaultdict, Counter
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

TAU = 2.0 * math.pi
SR_39K = 39062.5
SR_44K = 44100.0
SR_48K = 48000.0
RIM_DB = 96.0

def armadillo_decode(k_angle, k_radius, fs):
    # k_angle in 0..255 -> 10 octaves from fs/2048 to fs/2
    # theta' in [0, pi]
    # theta = pi * 2^(10 * (k_angle/255 - 1))
    # fc = (fs / 2) * 2^(10 * (k_angle/255 - 1))
    theta_prime = math.pi * (k_angle / 255.0)
    fc = (fs * 0.5) * (2.0 ** (10.0 * (k_angle / 255.0) - 10.0))
    
    # k_radius in 0..255 -> R' in [0, 96 dB]
    if k_radius == 255:
        # Check if unit circle null
        r = 1.0
        r_prime_db = float('inf')
    else:
        r_prime_db = RIM_DB * (k_radius / 255.0)
        r = 1.0 - 10.0 ** (-r_prime_db / 20.0)
        
    return fc, r, theta_prime, r_prime_db

print("ARMAdillo analytic decode function defined.")

# Test on k values:
print("k_angle=255 at 39.0625k -> fc =", armadillo_decode(255, 128, SR_39K)[0])
print("k_angle=0   at 39.0625k -> fc =", armadillo_decode(0, 128, SR_39K)[0])
print("k_angle=255 at 44.1k    -> fc =", armadillo_decode(255, 128, SR_44K)[0])
print("k_angle=255 at 48k      -> fc =", armadillo_decode(255, 128, SR_48K)[0])
