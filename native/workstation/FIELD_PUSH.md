# FIELD and PUSH

Design of 2026-09-07, night. Not yet ruled. HEADSPACE_SPEC.md still stands until Tyson cuts it.

In a serial cascade section gains multiply and responses add in dB; a corner is six sections of
five words, sixty bytes; a body is four corners and the chip's lerp; the ear decides.

## Two operations, never conflated

FIELD is the persistent eight-corner cube. Each vertex is an anchor: a named acoustic space
(a vowel, a metal, a cavity, a comb) carrying a pole-only scaffold, angle and radius held apart
in the words. The puck and Z play `interpolate_words` of the eight.

PUSH is a lever, summoned and dismissed. Its left end is what plays now, frozen as words. Its
right end is a foreign anchor. The diamond slides along the chip's own word interpolation, and
STAMP copies the words under the diamond into one of the four shipping corners.

The four shipping corners are the body. They are the only thing W writes.

## The glass

One surface, one font, charcoal. No panes, no popups, no modal state except the lever.

```
+----------------------------------------------------------------------------------------------+
| FIELD  VowelSpace                     plays: FIELD 0.42 0.71 z0.30        BODY  A  B     W   |
|                                                                                 C  D         |
|                                                                                 [pad]        |
|   [VOWEL /ae/  ]--------------------------------------------[METAL Bell 4.2k]                |
|   [VOWEL /i/   ]  z1                                    z1  [COMB 65 Hz     ]     Z          |
|        |                                                         |                |          |
|        |                       .                                 |                |          |
|        |                      ( )  puck                          |                +          |
|        |                       '                                 |                |          |
|        |                                                         |                |          |
|   [CAVITY Tube ]--------------------------------------------[VOWEL /u/      ]     |          |
|   [METAL Gong  ]  z1                                    z1  [COMB 220 Hz    ]                |
|                                                                                              |
|   MORPH ->    Q ^    Z 0.30                                                                  |
|                                                                                              |
|  ==============================  keyboard, three octaves  ==================================  |
|  pluck  saw  noise  loop     FIXED / KEY      A3                                              |
+----------------------------------------------------------------------------------------------+
```

The cube is one square. Each of its four positions carries two stacked tags: the z0 anchor on
top, the z1 anchor beneath. Ink weight on the pair follows Z, so at Z 0 the top tags are ink and
the bottom tags dim, at Z 1 the reverse. The Z rail stands to the right of the square. The puck is
MORPH across and Q up. A corner's name is its identity: family in capitals, then the instance.

The body is a small 2x2 at the top right, letters A B C D, each cell a name. It is also a pad:
drag on it and the plugin's MORPH and Q play the four shipping corners. The header word `plays`
says which surface is sounding, FIELD or BODY, with its coordinates. The last surface touched
plays. Click a letter to hear that corner alone.

### A vertex opened

Click a tag. The tag's edge of the square slides open into a strip, in place, and the square
narrows to make room. Nothing floats, the keyboard still plays, Esc or a click on the glass
closes it.

```
|   [VOWEL /ae/  ]  <- open                                                                    |
|   VOWEL  METAL  CAVITY  COMB  P2K  MORPHEUS            scaffold families, one lit            |
|   /i/  /I/  /e/  /ae/  /a/  /o/  /u/  /U/  /^/  /@/     instances, one lit                   |
|   1  ---o------------  4220 Hz   ---o---- 0.982                                              |
|   2  --o-------------   961 Hz   ------o- 0.994          six rows: angle rail, radius rail    |
|   3  -------o--------  2431 Hz   ----o--- 0.990                                              |
|   4  ----------o-----  3207 Hz   --o----- 0.971                                              |
|   5  o---------------   611 Hz   -----o-- 0.991                                              |
|   6  ------------o---  4936 Hz   ---o---- 0.985                                              |
```

Picking an instance loads its scaffold: the six pole pairs by byte copy, the zero words parked at
DC unity, the fifth word kept. Each row has an angle rail in Hz on a log scale and a radius rail.
They write words 2 and 3 through `words_from_geometry` and never touch each other. The strip
shows the vertex's response as a thin line above the rows while it is open.

### PUSH, the lever

Press P, or drag any card or tag onto the puck. A lever appears along the bottom of the square,
above the MORPH line, and stays until Esc or STAMP.

```
|   PUSH   [what plays: FIELD 0.42 0.71 z0.30] ---------------------------- [METAL Bell 4.2k] |
|                                              0%  ---------|---------  100%       8%          |
|          stamp: A  B  C  D                                                                   |
```

The left end is frozen: the words that were playing when the lever opened. The right end is a
slot: drop a tag, a card, or press a family and instance as in the vertex strip. The diamond is
the only control. What plays is `interpolate_words` between the two ends at the diamond, the
chip's linear lerp of its own log-compressed words, so a small percentage is a small musical
step. Left and Right arrows move it by 1%, Ctrl by 0.1%. The number reads in percent.

STAMP is the four letters under the lever. Press or click one and the words under the diamond
copy into that shipping corner, verbatim, no geometry roundtrip. The lever stays up so a second
corner can take a second percentage. Esc dismisses and FIELD resumes at its puck.

## The interaction, end to end

1. FIELD opens with the last session's eight anchors, puck and Z. Corner C of the body is empty.
2. Click the tag at the top left, z0. The strip opens. Press VOWEL, then /ae/. The vertex now
   carries that scaffold, poles only. The puck's sound changes at once if it is near that vertex.
3. Drag row 2's angle rail from 961 Hz to 1,100 Hz. The radius rail does not move. Close with
   Esc.
4. Move the puck and Z until the cube sounds right. The header reads FIELD with the coordinates.
5. Press P. The lever opens with the left end frozen at those words.
6. Press METAL, then Bell 4.2k. The right end fills. The diamond sits at 0%, the sound is
   unchanged.
7. Press Right ten times. The diamond reads 10%. The vowel has taken the bell's posture in the
   words, not a crossfade of two responses.
8. Press C. The body's cell C takes the name "/ae/ > Bell 10%" and the words under the diamond.
   The lever stays up.
9. Press Esc. FIELD resumes. Drag on the body pad to hear the four corners as the plugin will.
10. W writes the 240 bytes. Ctrl+S at any moment keeps what plays as a card.

## The data

```cpp
namespace hs
{
using Words6 = std::array<trench::core::PackedSection, 6>;

struct Scaffold
{
    juce::String family, name;
    Words6 words;
};

struct FieldVertex
{
    Scaffold scaffold;
    Words6 words;
    juce::String label() const { return scaffold.family + " " + scaffold.name; }
    void load (const Scaffold& s);
    void setAngle (int row, double hz);
    void setRadius (int row, double r);
};

struct Field
{
    std::array<FieldVertex, 8> vertex;
    float morph = 0.5f, q = 0.5f, z = 0.0f;
    trench::core::PackedBody packed() const;
    trench::core::CornerWords words() const { return packed().interpolate_words (morph, q, z); }
};

struct PushLever
{
    trench::core::CornerWords from;
    Scaffold to;
    juce::String fromName;
    float t = 0.0f;
    bool open = false;
    trench::core::CornerWords words() const;
};

struct Body
{
    std::array<Words6, 4> corner;
    std::array<juce::String, 4> name;
    float morph = 0.0f, q = 0.0f;
    trench::core::PackedBody packed() const;
    Bytes bytes() const { return packed().legacy_bytes(); }
    void stamp (int c, const trench::core::CornerWords& w, const juce::String& n);
};
}
```

`FieldVertex::load` copies words 2 and 3 of each row from the scaffold, writes words 0 and 1 as
the parked zero and keeps word 4. `setAngle` and `setRadius` go through `geometry_from_words`
and `words_from_geometry` on that row alone. `Field::packed` fills corner i of the PackedBody
with vertex i's six rows and the identity seventh. `PushLever::words` builds a PackedBody with
`from` in corners 0, 2, 4, 6 and `to` in corners 1, 3, 5, 7 and returns
`interpolate_words (t, 0, 0)`. `Body::stamp` copies the first six sections and refuses when the
seventh is not identity, because the shipping body has six.

Scaffold sources, all by byte copy: the 132 P2K corners, the 2,312 Morpheus corners whose
seventh section is identity, the Klatt and Hillenbrand vowels as already loaded, and the 83
Morpheus pole postures once they are exported with names.

## What to remove from HEADSPACE_SPEC.md

- The four quarters and the mother. FIELD's anchors are real corners, not an anchor pair made
  by transposition and relaxation. The FREQUENCY and STRESS rails go with them.
- The stage as a zero editor: squares, blade, carve, Alt-click, double-click to wake a row. The
  vertex strip edits angle and radius only. Zeros come later, as a separate act.
- The ARMAdillo plot.
- The spectrogram window and the Span mode.
- The palette as a pane. The vowel chart and the card list become the scaffold selector inside a
  vertex strip and the lever's right end.
- The engine's plot and the live spectrum.
- The patch routes KEY to FREQUENCY and VELOCITY to STRESS. Keep WHEEL to MORPH only.
- The queue items 1, 1c, 1d, 2, 3, 5 and 6. Keep 1a's one target and 4's ingest by byte copy.
- The later list, except "the vector: origin to anchor with a puck between, the push past the
  anchor", which is PUSH and is now the main road.

What stays: the law, the loop rewritten as the sequence above, W, Ctrl+S, undo, the keyboard,
the sources, FIXED or KEY, the document-and-projection rule through Bridge.

## Built, 2026-09-08

The page is rebuilt on Bridge: `Source/web/index.html` and `app.js`. The document grew
`Field`, `PushLever` and `plays` in Session (`Source/app/Field.h`, `Field.cpp`, `Session.cpp`),
stated and driven through `Bridge.cpp`. The cube is drawn oblique, eight vertices tagged by
family and name, a pole vertex a solid dot and a MASK vertex a ring. A vertex opens in place as a
plane of dots, angle across and radius up, each dot one row; angle writes word 2 and radius word
3, or words 0 and 1 for a mask. PUSH is a line from the puck to a far end; the far end is a card,
RELAX or TRANSPOSE; the diamond reads in percent; 1 to 4 or a drop on a bedrock cell stamps.
Esc hides the lever and keeps the sound. BEDROCK is the 2x2 with the plugin's puck, BAKE SLICE
and WRITE. Families: VOWEL, P2K, XL, 303, HRTF (47 SONICOM head keyframes), CAPTURES.
The field boots on extremes from the data: 303 closed, /a/, /i/, an XL bell, Tooth Comb as a
mask, Tooth Comb as a resonator, a head keyframe at 45 degrees up, schwa.
Not built: Morpheus-datum scaffolds, a named mask library, field persistence across sessions,
undo for angle and radius drags.
