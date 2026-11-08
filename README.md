# CA-2020 Chord Arpeggiator  (v1.0)

A MIDI chord machine + progression player + arpeggiator. It sits in front of any instrument
(JNO-600, SH-1010, any VSTi) and plays it. Standalone app has a built-in preview synth.

## Signal flow
Keyboard / clip -> **CA-2020** -> instrument (JNO-600 ...) -> mixer

| Host | How to connect |
|---|---|
| L-DAW 0.5 | Yellow **MIDI FX** slot on the track / mixer strip -> CA-2020 |
| Reaper | Put CA-2020 above the instrument in the same FX chain |
| Bitwig | Load as instrument, route its note output to the synth track |
| Cubase / Ableton / FL | CA-2020 on its own MIDI track; synth track takes MIDI input from that track |
| Logic | Not supported in v1 (needs an AU MIDI-FX build) |

Turn **PREVIEW SYNTH** off when it drives a real instrument (it is on by default only in the standalone app).

## Panel
**MODE**
- **CHORD** - one finger = full chord. *Auto* builds the right chord for that note in the key/scale (D in C major -> Dm); or force a type (m7, maj9, sus4, power...). Extension: triad / 7th / 9th.
- **PROGRESSION** - plays a chord progression. Trigger:
  - *Hold*: plays while you hold a key; with **KEY FOLLOW** the key you hold is the key of the song (hold A -> progression in A).
  - *Step*: every key press = next chord (great live).
  - *Host sync*: runs with the DAW transport, locked to the bar, no key needed.
- **MEMORY** - press **LEARN**, play any chord shape, release -> every key now plays that shape transposed.
- **THRU** - plain arpeggiator on the notes you hold.
- **LATCH** keeps playing after release; next key press starts a new chord.

**VOICING** - inversion, octave, strum (ms), spread (close / open / wide), bass note (root -1 / -2 oct), **VOICE LEADING** (picks the inversion nearest the previous chord so changes glide).

**FEEL** - velocity (as played / fixed), humanize (timing + velocity), swing, internal tempo (used when no host tempo).

**PROGRESSION** - 32 factory styles + 8 user slots. 16 step cells show chord, roman numeral and length.
Factory: click a cell -> *Copy to User n and edit*. User: click a cell -> root, chord type, length, insert, duplicate, delete; `+ STEP`, `- STEP`, `RENAME`.
Speed: half / normal / double.

**RHYTHM & ARPEGGIO** - Output:
- *Sustain* - chords held.
- *Rhythm* - 16 rhythm patterns (house offbeat, piano house, pop push, rock drive, trance gate, funk 16ths, reggae skank, lo-fi...). LED row: red = accent, orange = hit, bar = hold.
- *Arp* - Up, Down, Up/Down, Down/Up, Random, As played; 1/4 ... 1/32 incl. triplets; 1-4 octaves; gate; swing.

**SAVE / LOAD** - full presets (all settings + 8 user progressions + memory chord) as `.ca2020` files in *Documents/CA-2020 Presets*.
The 32 styles are also host *programs*, so a DAW can switch them.

## Factory styles
Pop: Pop Anthem, Pop Ballad, 50s Doo-Wop, Feel-Good Pop, Royal Road -
House: Deep House Stabs, Classic Piano House, Garage Organ, Melodic House, Nu-Disco -
Techno: Minor Stab Techno - Trance: Epic Trance, Uplifting Gate -
Rock: Classic Rock, Stadium Rock, 12-Bar Blues, Pop-Punk -
Synthwave: Night Drive, Outrun Sunset - Lo-fi: Lo-fi ii-V-I, Neo-Soul -
Jazz: Jazz Turnaround, Bossa - R&B: Smooth R&B, Gospel Lift -
EDM: Future Bass, Festival Anthem - Funk: Dorian Funk Vamp - Reggae: One Drop -
Cinematic: Epic Cinematic, Andalusian - Classical: Canon

## Code layout
- `Source/engine/` - the musical engine in plain C++ (no JUCE): theory, styles, chord/arp engine, preview synth.
  Tested off-line with `tools/engine_test.cpp` (`g++ -O2 -std=c++17 tools/engine_test.cpp -o t && ./t`).
- `Source/PluginProcessor.*` - JUCE plug-in wrapper, parameters, presets.
- `Source/PluginEditor.*` - panel.
- `Source/LFamilyLook.h` - shared L-family look (identical copy lives in L-DAW).
