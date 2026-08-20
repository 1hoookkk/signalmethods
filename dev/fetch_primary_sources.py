"""
dev/fetch_primary_sources.py
Downloads open-access cleanroom primary sources (PDFs/papers) directly into recipes/sources/.
"""

import os
import urllib.request
import ssl
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES_DIR = ROOT / "recipes" / "sources"

SOURCES = [
    {
        "filename": "Hillenbrand_1995_Vowel_Acoustics.pdf",
        "url": "https://homepages.wmich.edu/~hillenbr/Papers/HillenbrandEtAl1995.pdf",
        "desc": "Hillenbrand et al. 1995 American English Vowel Formant Trajectories"
    },
    {
        "filename": "Fujimura_1964_Sinusoidal_Response_Vocal_Tract.pdf",
        "url": "https://www.speech.kth.se/prod/publications/files/qpsr/1964/1964_5_1_001-010.pdf",
        "desc": "Fujimura & Lindqvist 1964 Vocal Tract Transfer Function & Nasal Zeros"
    },
    {
        "filename": "Klatt_1980_Cascade_Formant_Synthesizer.pdf",
        "url": "https://www.cs.cmu.edu/~dod/papers/klatt80.pdf",
        "desc": "Klatt 1980 Cascade Formant Synthesizer Architecture"
    },
    {
        "filename": "Peterson_Barney_1952_Vowel_Formants.pdf",
        "url": "https://www.seas.upenn.edu/~cis5210/lectures/vowels_PB.pdf",
        "desc": "Peterson & Barney 1952 Vowel Formant Reference Dataset"
    },
    {
        "filename": "US5170369_Rossum_ZPlane_Filter.pdf",
        "url": "https://patentimages.storage.googleapis.com/00/4c/32/3858c42ca138ba/US5170369.pdf",
        "desc": "US Patent 5,170,369 (Rossum 1992 Z-Plane Filter)"
    },
    {
        "filename": "US10514883_Rossum_Morphing_Filter.pdf",
        "url": "https://patentimages.storage.googleapis.com/62/1f/ff/88448ebf5c7170/US10514883.pdf",
        "desc": "US Patent 10,514,883 (Rossum 2019 Cascade Morphing Filter)"
    },
    {
        "filename": "US5952599_Rossum_Interpolating_Filter.pdf",
        "url": "https://patentimages.storage.googleapis.com/fa/27/ef/52a8a5b28dca2a/US5952599.pdf",
        "desc": "US Patent 5,952,599 (Rossum 1999 Multi-dimensional Parameter Interpolation)"
    },
    {
        "filename": "Morpheus_Manual_1993.pdf",
        "url": "https://cdn.shopify.com/s/files/1/0277/4548/4865/files/Morpheus_manual_170206.pdf?v=1785520097",
        "desc": "E-mu Morpheus Hardware Operation Manual (289 Cubes)"
    }
]

def fetch_all():
    SOURCES_DIR.mkdir(parents=True, exist_ok=True)
    
    # Allow standard TLS context
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    
    opener = urllib.request.build_opener(urllib.request.HTTPSHandler(context=ctx))
    opener.addheaders = [('User-Agent', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)')]
    urllib.request.install_opener(opener)

    print(f"Fetching cleanroom primary sources into: {SOURCES_DIR}\n")
    
    for item in SOURCES:
        target = SOURCES_DIR / item["filename"]
        if target.exists() and target.stat().st_size > 1024:
            print(f"[EXISTS] {item['filename']} ({target.stat().st_size // 1024} KB)")
            continue
        
        print(f"[FETCHING] {item['desc']} ...")
        try:
            urllib.request.urlretrieve(item["url"], target)
            print(f"  -> Saved {item['filename']} ({target.stat().st_size // 1024} KB)")
        except Exception as e:
            print(f"  -> FAILED: {e}")

    print("\nFetch completed.")

if __name__ == "__main__":
    fetch_all()
