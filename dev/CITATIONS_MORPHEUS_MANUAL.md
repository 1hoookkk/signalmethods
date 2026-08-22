# Citations — Rossum Morpheus (eurorack) manual

Extraction only. Verbatim quotes, no reasoning. Source:
`C:\Users\hooki\Downloads\emu_re_artifacts\Rossum_Morpheus_manual_170206.pdf`,
80 pages, text layer present. Printed page numbers and PDF indices agree.

This is the **Rossum Electro-Music eurorack module** manual, not the 1993 E-mu
Morpheus manual. Entries are numbered `C###` and called "Cubes". PDF page 5 is
artwork only.

---

## The topology, stated outright

**p. 6, "Some Details":**
> "A FREQUENCY RESPONSE: This is the most basic construct of a Morpheus filter.
> It is essentially a single static configuration of Morpheus's 14 poles and
> zeros. For example, a frequency response might be configured as **six
> independent bands of parametric EQ and a low pass section**, as in Figure 1,
> above."

**p. 6, Figure 1 caption block, as extracted:**
> "IN OUT
> FC Q | FC BW GAIN | FC BW GAIN | FC BW GAIN | FC BW GAIN | FC BW GAIN | FC BW GAIN
> 1 Low Pass Section
> 6 Parametric Equalizer Sections
> FIGURE 1: A FREQUENCY RESPONSE"

**p. 7, "Some Details" (continued):**
> "By varying that single CV-controllable parameter, you're actually
> interpolating between **20 different frequency, bandwidth, resonance, and gain
> parameters** simultaneously."

**p. 6, "What is Morpheus?":**
> "Morpheus is a unique digital filter module inspired by the **14th Order
> Z-Plane Filter** Dave originally invented for E-mu's fabled Morpheus
> synthesizer back in the mid-1990s."

> "Each Cube is composed of up to 8 complex frequency response configurations
> that you can picture as being at the corners of a three dimensional cube
> (hence the name)."

> "Due to processor limitations back in the day, the original Morpheus was
> capable of realtime morphing in one dimension, but interpolation in the
> frequency and transform dimensions were set at note-on and remained static for
> the remainder of the note."

## What `.4` means, stated outright

**p. 7:**
> "In this case, the two parameters (Frequency and Morph) define a point on a
> virtual plane that specifies the interpolation between the four Frequency
> Responses. In Morpheus, configurations of this type are identified with a
> **'.4' at the end of their Cube name**."

**p. 8, "A Few Words about Distortion":**
> "All of the Morpheus Filters that are based on '.4' Cubes have by default the
> Transform control knob and CV input set to control distortion (since with only
> 4 Frequency Responses, there is no Transform axis to control anyway). However,
> even for the full 3D Cubes, you have the option of programming the Transform
> control and CV to control distortion, in which case the actual Transform value
> is set as a static value by the Transform CV Offset parameter in the Edit
> Filter Menu."

**p. 20, "Transform Controls Distortion":**
> "ANOTHER NOTE: By default, all factory filters that end in '.4' are set to
> Transform Controls Distortion. Since these filters consist of only 4 frequency
> configurations, controlling the Transform axis generally has a less
> interesting effect."

## Counts

**p. 6:** "The Morpheus module includes **289** meticulously crafted frequency
response combinations, what we call 'Cubes.'"

**p. 77, "12. Specifications":**
> "FILTER CUBES 289 / FILTERS 1000 / FILTER SEQUENCES 200 / SEQUENCE STEPS
> 20,000 Dynamically Allocated"

The Chapter 11 list as printed runs **C000 through C280 with no gaps — 281
entries.** The stated total is 289.

**p. 17:** "NOTE: A special case is Cube 000, the Null Cube. The Null Cube
provides no processing, so you can think of it as a bypass of sorts."

**p. 29:** "...by bringing up #289 Null Filter..."

## Interpolation controls

**p. 17–18, "Frequency, Morph, and Transform CV Offsets":**
> "The Frequency, Morph, and Transform CV Offsets let you define the
> interpolation point on each respective axis that results from a cumulative CV
> of 0V... Their range is from -5.0V to +5.0V. The default value for all axes is
> 0.0V."

**p. 18:**
> "ANOTHER NOTE: The interpolation point can never extend beyond the edge of the
> 3D Cube space."

**p. 19:** "Left and Right Distortion... The range is from -30dB to +70dB."
**p. 19:** "Left and Right Gain... Their range is -40dB to +40db in 1dB
intervals."

**No sample rate, word size, section count per cascade, or bit depth appears
anywhere in this PDF.**

## Family headers, verbatim (all under "11. Factory Cubes")

**p. 32, "Flangers"** — header printed with no paragraph beneath it.

**p. 36, "Dipthong Filters":**
> "Implemented with parametric equalizer subsections, the resonances do not have
> the traditional overall low pass effect that a true vocal resonance would
> have. Instead, they are placed at the same frequency as the resonances would
> be found in a true vowel, but the response at high frequencies is essentially
> flat to allow high frequencies of the signals to get through the filter.
> The Morph axis controls movement between vowels. Brightness and depth of
> effect is controlled by Transform. In certain filters, brightness is
> controlled by the Frequency CV. Each paravowel 'A' contains peaks at around
> 800 1150, 2800, 3500 and 4950 Hz. Each paravowel 'E' contains peaks at around
> 400, 1600, 2700, 3300 and 4900 Hz. Each 'O' has peaks at around 450, 800,
> 2830, 3500 and 4950 Hz. Each 'U' has peaks at around 325, 700, 2530, 3500 and
> 4950 Hz."

**p. 39, "Standard Filters":**
> "These filters are variations on traditional 2 and 4-pole synthesizer filters"

**p. 44, "Equalization Filters":**
> "Most of these filters are variations of traditional parametric EQ filters."

**p. 45, "Complex Filters":**
> "Many of these filters have never existed for musical applications before!"

**p. 63, "Distortions":**
> "Jump Back! Transform is used to control the distortion filters."

**p. 63, "Vari-Pole Filters":**
> "This group of lowpass filters all have a variable slope. Variable slope
> filters are very rare in synthesizers (except, of course, in the Rossum
> Evolution Variable Character Ladder Filter module), but the effect of the
> variable slope occurs in natural sounds. The number of poles relates to the
> maximum steepness of the filter."

**p. 64, "Tracking Filters":**
> "These filters range from lowpass to resonant peaking filters. The Frequency
> parameter allows the filter to 'track' specific harmonics as you play up and
> down the keyboard..."

**p. 65, "Parametric Tracking Filters":**
> "These filters are set up with various combinations of peaks and notches. The
> Frequency parameter controls functions such as bandwidth, amount and cutoff
> frequency."

**p. 66, "Harmonic Shifters":**
> "These filters are designed to alter the normal harmonic relationships of
> instruments by radically shifting the resonant frequency bands using the Morph
> parameter."

**p. 68, "Vocal Formants":**
> "These cubes are designed to simulate human vocal resonances. In these cubes,
> all axes control the movement between vowels. Each filter allows a slightly
> different type of vowel shifting control."

**p. 69, "Instrument Formant Filters":**
> "These filters are to simulate the resonant characteristics of various types
> of instruments... The subcategories of instrument formant filters are in the
> following order: Keyboards, Strings, Plucked, Wind, Brass, Percussion."

**p. 74, "Miscellaneous Filters":**
> "A potpourri of unique filters that don't fit in any other category."

Note: "Notches!" on p. 43 is printed *after* the C066 entry line and reads as
that entry's description, not as a family header.

## The Martens connection, named in the manual

**p. 39, C043 VowelSpace:**
> "**Dr. William Martens** inspired this filter cube, which allows any vowel to
> be approximated with only two parameters.
> Frequency: Sweeps the second formant from 500Hz up to 2500Hz.
> Morph: Sweeps the first formant of the vowel from 300Hz up to 850Hz.
> Transform: Changes the vowel 'stress.' Low stress means that all of the vowel
> frequencies collapse to a relaxed 'schwa' sound. Maximum Transform produces
> the maximum excursion (stress) for all of the vowel frequencies."

## Selected per-filter construction passages

**C003 Flange2.4, p. 32:** "This flanger has a series of 6 notches placed at
octaves, not linearly. With all CVs at minimum, the notches are around 56, 112,
225, 450, 900, and 1800 Hz."

**C005 Flange3.4, pp. 32–33:** "The notches start at about 40Hz, and the higher
ones are spaced at a ratio of 1.61 between notch frequencies. This spacing means
that notches will not fall on top of all of the harmonics of a sound at once...
Traditional flangers have notches spaced linearly, not log spaced."

**C001 LPFlange.4, p. 32:** "Frequency: Starts with high end roll-off and deep,
narrow notches around 60, 120, 230, 460 and 920 Hz and transforms into less
deep, wider notches around 12, 14, 16, 18 and 19 kHz with no roll-off."

**C053 2pole>4pole, p. 41:** "Transform: Changes from a 2-pole filter
(-12dB/oct.) to a 4-pole filter (-24dB/oct.)."

**C079 4>2 LowQHiQ, p. 45:** "This All Pole filter is modeled on traditional low
pass filter types... Morph: Changes from a 4-pole filter (-24dB/oct.) to a
2-pole filter (-12dB/oct.)"

**C124 500up.4, pp. 50–51:** "Poles and zeros alternate, spaced at 500Hz
intervals. Increasing Morph boosts the peaks slightly, and greatly deepens the
notches, producing an effect similar to band-pass filtering."

**C060 HighAccent.4, p. 42:** "The center frequencies remain constant throughout
all permutations of this filter along the Morph and Frequency CVs; what changes
are the 'Q' settings for the various poles and zeros."

**C155 HighsTwist.4, p. 56:** "The frequencies of the poles and zeros in this
cube remain constant as Morph or Frequency are modulated, but the 'Q' values are
modulated in a variety of ways."

**C182 Comb Voices, p. 60:** "Center frequencies of the poles and zeros in this
filter can move in opposite directions as the filter is modulated along any of
its axes, producing an amazing variety of possible curves."

**C198 Poles 1-7, p. 63:** "Morph: Low values produce 1 pole filter; high values
produce 7 pole version."

**C204 Multipole, p. 64:** "Transform: Higher values increase slope and
resonance and **re-order some poles**. At max, peaks tuned to octaves can be
swept along the Morph axis."

**C148 Skweezit, p. 55:** "Varying the Morph parameter varies the 'Q' settings
for most of the peaks and notches, while sweeping the frequency and Q of the
first pole."

## Names carrying a `.4` suffix, exactly as printed

LPFlange.4, LPFlangBk.4, Flange2.4, Flange3.4, Flange4.4, Flange6.4, Flange6R.4,
Flange7.4, BriteFlnge.4, AUParaVow.4, UOParaVow.4, SftEOVowl.4, C1-6Harms.4,
Voce.4, ChoralComb.4, Bassutoi.4, Be Ye.4, Ee-Yi.4, Ii-Yi.4, Uhrrrah.4,
YeahYeah.4, YahYahs.4, YoYo.4, BrickWaLP.4, 2 PoleLoQ.4, 4PoleLoQ.4,
4PoleMidQ.4, Low Past.4, APass.4, HPSweep.4, HiSwept1.4, HghsSwpt2.4,
HighAccent.4, HiPassSweep.4, DeepCombs.4, Notcher 2.4, Ntches2Oct.4, VarSlope.4,
BassEQ 1.4, B BOOST.4, BssBOOST2.4, Wah4Vib.4, Wa Wa.4, BrassRez.4, BrsSwell2.4,
Chiffin.4, EZ Vibez.4, Piano 2.4, MoogVocodr.4, StrSweep.4, Qbase.4, AcGtrRs.4,
Tube Sust.4, G MajTrans.4, HOTwell.4, Ev/OdNtch.4, NotchPeaks.4, LoudBoost.4,
500up.4, PeakSweep.4, TSweep.4, SweepHiQ1.4, V>FcQuad.4, Nexus.4, Krators.4,
Harmonix.4, GreenWorld.4, Comb/Swap.4, Comb/HP.4, Cavatate.4, GentleRez.4,
Lo/High.4, SubtleMove.4, BuzzyPad.4, Bw5kHz+6.4, Bw65Hz/2k.4, Bb80Hzbw1.4,
HighsTwist.4, HarmSweep.4, EvenCuts.4, OddCuts.4, PWMtrans.4, HiEndQ.4,
BroadRes.4, ResoHose.4, Syn Wow.4, CombSweep.4, Diffuser.4, AHmBnd.4,
PoleCross.4, ApDistB6.4, InFifths.4, Piano LP.4, Quartet.4, Mellotron.4,
Pluck.4, FlutBreth.4, VClarinet.4, New Mute.4, Trempeto.4, SfBrzando.4,
UduFilter.4, CableRing.4, SnakeCros.4, Analog.4

Names ending in a bare `4`, no dot: `EZ Rhodez4` (p. 47), `StrngThing4` (p. 47),
`MdlySweep4` (p. 59).

Printing irregularities to preserve: `C 106 Tube Sust.4`, `C 111 HOTwell.4`, and
`C125C1Harmonic` (no space between number and name).

## E-LoaderOperationManual.pdf

16 pages, text layer present. Search counts across the whole document: "filter"
0, "ROM" 0, "cube" 0, "Z-Plane" 0, "flash" 0, "format" 0. The only "Filter" hits
are on p. 13 and refer to MIDI message filtering. **Nothing about filter data
format, banks, or ROM layout.**
