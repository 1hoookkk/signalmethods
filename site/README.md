# TRENCH product page

One static page. Upload this folder as it is to any static host (Cloudflare Pages, Netlify, GitHub Pages: drag the
folder, done) and point the domain at it.

## Your recordings
assets/clips/ holds the two pairs recorded on 2026-09-04, level-matched to -16 dBFS RMS so Dry and Processed
compare at the same loudness:

    brass_dry.wav   brass_proc.wav
    reese_dry.wav   reese_proc.wav

To add a sound, copy one pm-row block in index.html, change data-ex and data-led to the new name, add the name to
the `names` table in the script, and drop <name>_dry.wav and <name>_proc.wav into assets/clips/. 16-bit 44.1 kHz,
mono or stereo. Until a file is present its buttons say "recording not here yet".

## Trying it on this machine
Browsers refuse to fetch the clips from a file:// page. Serve the folder instead:

    python -m http.server 8080 --directory C:\Users\hooki\trench-native\site

then open http://localhost:8080/ .

## The two links
Buy Now and Download Demo are href="#" in index.html (search for pm-btn-buy and Download Demo). Put the store's
checkout link and the demo installer link there.

## Names
The product name, the tagline, the company name, the support address and the footer are plain text in index.html.
