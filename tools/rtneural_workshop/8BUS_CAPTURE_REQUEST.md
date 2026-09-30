# Mackie analog 8•Bus hardware reference request

Prepared 2026-09-30. Draft only: no message sent, purchase made or capture session booked.

## First source to approach

[PastToFutureReverbs — Mackie 32 8 Analog Console, Channel & Bus](https://pasttofuturereverbs.gumroad.com/l/hdiqg)

The creator's listing advertises channel and bus captures for NAM and Proteus, plus IRs. The displayed price was $15 when checked. The listing does not establish whether original training WAVs are included, which exact routes and control settings were captured, or whether training and distributing a derivative model is permitted. Its capture quality has not been independently verified here. Buying a finished model alone would not supply a physical-hardware reference for our own accuracy tests.

A secondary hardware-access lead is [Nashville Connection / Mark Dreyer Productions](https://www.nashvilleconnection.com/studio-gear/), whose own gear page lists a Mackie 32×8 console. Current ownership, working condition, willingness to make captures and pricing are unconfirmed.

## Ready-to-send inquiry

Hello,

I'm developing a real-time model of the original analog Mackie 8•Bus, with particular interest in driven channel-input behavior. I found your Mackie 32 8 channel and bus capture pack.

Does the pack include the original dry stimulus and unprocessed hardware target WAVs used to train the models? Could you identify the desk revision, input/output route, trim settings and analog level calibration for each capture?

If the original recordings are not included, would you license them separately or quote for a fresh capture session? The first target is balanced line input to channel insert send, isolating the input stage. We would also like a separately documented full channel-to-main-output reference, with EQ bypassed or its exact settings recorded, to check what the later desk stages contribute.

We need clean, onset-of-clipping and hard-overload recordings at documented trim/input levels, plus separate bass/transient examples that were not used for training. Repeating the same held-out take twice without changing settings would let us measure the desk's own noise and repeatability. A converter loopback and calibration tone would establish interface delay and level. The desk should clip where intended while the recording converter retains headroom.

Please keep the WAVs unnormalized and free of additional processing, and include the interface settings and any attenuation after the desk. Mono 48 kHz / 24-bit or float is preferred; preserve originals if an existing capture uses another rate. We have a 190-second NAM calibration stimulus ready to provide.

Could you also state the permission and price for training our own model and distributing its weights in a software product, separately from any permission to redistribute the recordings?

Thank you.

## Reference acceptance

- Confirm an original analog 8•Bus, with the unit/channel and relevant board revision documented. Other Mackie ranges and the digital d8b do not satisfy this target.
- Record input type, wiring, trim, filter/EQ state, selected output, faders, external load, interface analog ranges and any fixed attenuation. Keep channel-input and combined channel/bus routes identified separately.
- Keep the exact dry source and raw target. Establish round-trip latency, polarity and any clock drift from calibration; do not remove the desk's own phase response or silently fit away gain error.
- Distinguish DAC/ADC behavior from desk behavior using the loopback measurement. Calibration scaling must be explicit and traceable; no automatic leveling or normalization.
- Include level coverage from clean through severe overload, low-frequency sustained notes, transients, silence and overload recovery. Capture each trim setting as its own documented condition.
- Hold out separate musical takes from training and checkpoint selection. Use repeated hardware takes to measure the practical noise/repeatability floor before setting numerical acceptance thresholds.
- Train against the hardware targets with MR spectral loss + LF ESR. Check waveform, spectrum, phase, harmonics, recovery, silence and blind A/B listening on the held-out set. Confirm the exported RTNeural output agrees with the trained model separately.

The current SPICE model remains a provisional circuit hypothesis: its 2SA1084 and NJM4560 overload behavior includes estimates. The existing LSTM's agreement with that SPICE teacher does not establish agreement with an 8•Bus. Finished third-party models can be audition references once lawfully obtained, but distillation also inherits their errors.
