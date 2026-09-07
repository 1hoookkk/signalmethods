# TRENCH corpus atlas

In a serial cascade section gains multiply and responses add in dB; a corner is six sections of five words, sixty bytes; a body is four corners and the chip's lerp; the ear decides.

- nodes: 2444 (132 P2K, 2312 Morpheus)
- A/B/C pairs computed: 2985346
- sweep (D) pairs computed: 59908 (30 nearest by A+B per node, deduplicated)
- edges kept: 9280
- candidate pairs rejected, instability (pole radius > 0.9999 mid-sweep): 8051
- candidate pairs rejected, excursion > 12.0 dB: 6835
- candidate pairs passing both gates: 45022
- connected components: 151; largest 2294 nodes; component sizes top ten: 2294, 1, 1, 1, 1, 1, 1, 1, 1, 1
- five nodes with most neighbours: 037_Uhrrrah.4 c0 (303), 037_Uhrrrah.4 c2 (302), 037_Uhrrrah.4 c1 (300), 040_Vow_Vow2 c0 (299), 037_Uhrrrah.4 c3 (298)
- longest geodesic in largest component: 360.874, 217_MltiMetricC c6 <-> 044_BrickWaLP.4 c6
- edge cost D_total = A + 1.0*B + 0.5*C; A RMS dB over 64 ERB centres 50 Hz to 16 kHz, mean-removed; B and C mean assignment cost in (log2 Hz, semitone bandwidth), absent pair 4.0
- Morpheus nodes use the 6 lowest-frequency active pole sections of 7; the dropped section per node is in atlas.json

## Family positions (UMAP)

- COMPLEX FILTERS n=648 umap centroid (3.11, 7.83) spread 10.38
- DIPTHONGS n=184 umap centroid (3.83, 8.47) spread 10.30
- EQUALIZATION FILTERS n=48 umap centroid (-0.72, 6.19) spread 2.61
- FLANGERS n=168 umap centroid (4.70, 9.36) spread 8.77
- STANDARD n=208 umap centroid (0.66, 7.14) spread 16.12
- UNLISTED n=1056 umap centroid (0.79, 5.84) spread 9.95
- P2K n=132 umap centroid (2.43, 5.01) spread 3.03

## Family geodesics inside the largest component

- COMPLEX FILTERS n=611 median intra-family geodesic 48.25, median to the rest 49.05, ratio 0.98
- DIPTHONGS n=184 median intra-family geodesic 51.41, median to the rest 51.13, ratio 1.01
- DST n=4 median intra-family geodesic 85.31, median to the rest 147.44, ratio 0.58
- EQ+ n=24 median intra-family geodesic 44.42, median to the rest 49.30, ratio 0.90
- EQ- n=8 median intra-family geodesic 58.80, median to the rest 56.06, ratio 1.05
- EQUALIZATION FILTERS n=48 median intra-family geodesic 35.30, median to the rest 44.35, ratio 0.80
- FLANGERS n=167 median intra-family geodesic 45.83, median to the rest 48.45, ratio 0.95
- FLG n=8 median intra-family geodesic 71.07, median to the rest 66.17, ratio 1.07
- LPF n=20 median intra-family geodesic 51.27, median to the rest 53.43, ratio 0.96
- PHA n=8 median intra-family geodesic 132.56, median to the rest 104.01, ratio 1.27
- REZ n=28 median intra-family geodesic 53.90, median to the rest 54.26, ratio 0.99
- SFX n=4 median intra-family geodesic 86.58, median to the rest 69.03, ratio 1.25
- STANDARD n=206 median intra-family geodesic 39.70, median to the rest 45.56, ratio 0.87
- UNLISTED n=946 median intra-family geodesic 48.33, median to the rest 49.33, ratio 0.98
- VOW n=24 median intra-family geodesic 48.04, median to the rest 56.47, ratio 0.85
- WAH n=4 median intra-family geodesic 51.10, median to the rest 52.14, ratio 0.98

## Continents

The graph holds 2444 nodes in 151 components; the largest carries 2294 of them (93.9%), 150 nodes are isolated singletons, and the longest geodesic across the largest component is 360.874 in edge-cost units over 29 hops, median kept-edge cost 8.047. Median intra-family geodesic against median geodesic to the rest of the component, by family: COMPLEX FILTERS 48.25/49.05, DIPTHONGS 51.41/51.13, DST 85.31/147.44, EQ+ 44.42/49.30, EQ- 58.80/56.06, EQUALIZATION FILTERS 35.30/44.35, FLANGERS 45.83/48.45, FLG 71.07/66.17, LPF 51.27/53.43, PHA 132.56/104.01, REZ 53.90/54.26, SFX 86.58/69.03, STANDARD 39.70/45.56, UNLISTED 48.33/49.33, VOW 48.04/56.47, WAH 51.10/52.14. Rejected candidate sweeps: 14886 of 59908 (24.8%). Of the labelled families only DST (ratio 0.58), EQUALIZATION FILTERS (0.80), VOW (0.85), STANDARD (0.87) and EQ+ (0.90) sit more than 10 per cent closer to themselves than to the rest; COMPLEX FILTERS, UNLISTED, DIPTHONGS, FLANGERS, REZ, LPF and WAH all land between 0.95 and 1.01, and PHA (1.27), SFX (1.25), FLG (1.07) and EQ- (1.05) are further from themselves than from the corpus. One component holds 2294 of 2444 nodes and the other 150 are singletons whose 30 candidates all failed the sweep gates, so the corpus is one continent with a dense interior rather than separated landmasses; inside it the family structure is a gradient, with the vowel, equalisation and P2K regions the only ones that hold together and the phaser, flanger and SFX corners scattered through the whole.
