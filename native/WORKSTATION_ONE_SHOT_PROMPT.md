# Handoff prompt for the outside model

One pass, one shot. Paste the text below into GPT-6 Astra at High reasoning with this
repository attached.

---

You are the senior engineer building the TRENCH Workstation, in one pass, from a written
specification. The repository is attached.

Read native/WORKSTATION_ONE_SHOT_SPEC.md in full first. It is complete: the framework (a
MATLAB R2025b toolbox over two MEX files), what to keep, delete and create, the laws, the
data formats, the C++ signatures you call, the two MEX contracts, the four rooms, the bank
as a line of anchors, the acceptance tests and the build. After the spec, read only what it
points you to: the headers in native/workstation/Source/model/, the four core headers named
in section 6.2, native/workstation/Tests/MorphTests.cpp, native/WORKSTATION_EDIT.md, and the
Spectrogram README and two captures under evidence/research-results/emu-sgi-1993/spectrogram/.
Nothing else in the repository is needed; the old shell is deleted unread.

The spec fixes the laws, the names, the contracts and the acceptance thresholds. Everything
else is yours: the layout of the panel and the display, the MATLAB graphics strategy, the
MEX packaging, the real-time engine, the test structure. Bring your own expert direction to
those. Where you know a better way within the laws, build it and record it in
native/workstation/DECISIONS.md with one line of reason. Where the spec is silent, decide
and record. Where the spec contradicts itself or the code it keeps, resolve toward the laws
in section 4 and record the contradiction. Never stop to ask.

Spectrogram is the target: a control panel of grouped switches, number boxes and sliders
beside one display, in the MATLAB palette the spec gives. Every control carries its literal
name from section 9 or no text. Every edit to a frame goes through the chord. Everything
the tool plays, draws and writes comes from the one Live value.

Build all of it: the deletions in section 3, the CMake targets and the vcvars wrapper, both
MEX files, the MATLAB toolbox laid out like a MathWorks toolbox repository, the factory
banks, the tests, the README, and the record updates in section 13. Then run the acceptance
in section 10 headless: the two C++ suites unchanged and green, then tBridge, tRooms and
tShot under matlab -batch, registered in ctest and in buildfile.m. Make every check green by
fixing code. Never open a window during a test.

Deliver: the finished repository as commits with explicit pathspecs (nothing under plugin/
except plugin/CMakeLists.txt and plugin/NEXT_SESSION.md), the count of passing checks in
every suite, the paths of the four shot PNGs, DECISIONS.md, and a short list, in plain
words for a musician, of what you found that the spec overlooked: in the tool, in the
method, or in the material.

The musician who owns this tool wrote the last line. Build to it.

"do not use abstract vague naming like capture etc. put the most literal naming you can or
do not include text."
