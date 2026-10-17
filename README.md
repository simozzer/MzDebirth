# MzDebirth

A rack of the Mz synths, played from a step sequencer, in the spirit of the old ReBirth
RB-338: five machines, each with its own panel, its own patterns and a channel on the mixer,
all on one clock, with ReBirth's effects units along the top.

![The rack playing: the transport and KEY section, the effects units, the Mz303 and the Mz101](docs/img/rack.png)

It plays the synths' real engines - [Mz303](https://github.com/simozzer/Mz303),
[Mz101](https://github.com/simozzer/Mz101), [Mz808](https://github.com/simozzer/Mz808),
[Mz909](https://github.com/simozzer/Mz909) and [Mz950](https://github.com/simozzer/Mz950) -
and its PCF is [MzPCF](https://github.com/simozzer/MzPCF)'s. Nothing is copied: each engine
is compiled straight from its own repository, so a fix to a synth or the PCF reaches the rack
too.

## Getting it

Download **MzDebirth-*version*-win64-Setup.exe** from the
[releases page](https://github.com/simozzer/MzDebirth/releases) and run it. It installs the
app, a Start menu entry, an uninstaller, and the Mz950's six sample disks; installing for
just yourself needs no administrator rights. The **win64.zip** beside it is the same, to run
without installing - keep the `disks` folder next to `MzDebirth.exe`.

Windows 10 or 11, 64-bit. The app is not code-signed, so Windows may warn about an unknown
publisher: choose **More info**, then **Run anyway**.

## Getting started

- **Options** (top left) picks the audio output.
- **PLAY**, or the space bar. The rack opens with the factory grooves, and every machine
  plays pattern A1.
- Pick another pattern on each machine's **PATTERN** pads - bank A to D, then 1 to 8. Picked
  while playing, it blinks, and starts when the pattern playing comes round. The factory
  banks are written so that pattern *n* of every machine plays with pattern *n* of the others:
  **A** acid house, **B** techno, **C** electro, **D** deep house; in each, 1-3, 6 and 7 are
  grooves, 4 and 8 fills, and 5 a breakdown.
- **NEW** brings the factory grooves back, or clears the rack to write your own.
- **SAVE** and **LOAD** keep whole racks - patterns, knobs, mixer and effects - in
  `.mzdebirth` files. The app also remembers where it was left.

## The machines

- **Mz303** - the full panel, and a piano-roll step editor with the 303's UP, DOWN, ACCENT
  and SLIDE on every step. Click a cell for a note, click it again for a rest, drag along
  to keep going. A slide holds the note over into the next step and glides to it.
- **Mz101** - the panel in its sections, less what the rack does for it (its arpeggiator,
  sequencer and clock). The same step editor without ACCENT, which the 101 has none of; its
  SLIDE is a legato, which glides when PORTAMENTO is on AUTO. SYNC on TEMPO locks its
  modulator to the rack.
- **Mz808** and **Mz909** - the full panels, a column per instrument, and the hardware's way
  of writing a beat: pick an instrument on the select row (or AC for the accents) and its
  steps show on the sixteen keys. An instrument with steps written has a bright edge.
- **Mz950** - plays S900/S950 disk images: **LOAD DISK**, then pick a programme. Its steps
  are a grid of notes, any number on a step, with ACCENT and TIE rows; a tied step carries
  on the notes it shares with the step before. The keys the programme plays are lit, and a
  drum kit's are named by their samples. It opens with the drum disk that comes with Mz950.

Every machine has four banks of eight patterns, of up to sixteen steps (**STEPS**), with
**COPY**, **PASTE** and **CLEAR**, and its own **SHUFFLE** switch: whether it swings with the
transport's SHUFFLE knob.

![The Mz808 and the Mz909, each with its pattern section and mixer strip](docs/img/drum-machines.png)

## Effects and the mixer

ReBirth's four units, along under the transport:

- **DIST** - drive, and a shape from soft, to hard, to a lopsided fuzz.
- **PCF** - a filter played by one of 32 step patterns: struck on every sixteenth at that
  step's level, closing again by DEC; low-pass or band-pass. The same as the
  [MzPCF](https://github.com/simozzer/MzPCF) plugin, to use on any track in a DAW.
- **DELAY** - echoes 1 to 16 sixteenths behind at the rack's tempo, or triplets, with
  feedback and pan. They ring on after STOP.
- **COMP** - ratio and threshold, for the channels switched into it, and for the whole mix
  with **MASTER**.

Each machine's mixer strip has its pan, its **DELAY** send, its switches into **DIST**,
**PCF** and **COMP** - in that order, before the fader - its fader and **MUTE**.

## Keys, for harmonic mixing

The **KEY** section names the key as Mixed In Key does, on the Camelot wheel: **1A-12A**
are minor, **1B-12B** major. Drag it or use the wheel to go round; click to swap A and B. It
shows the keys that mix with it, and the key of the bank the Mz303 is showing.

- **GENERATE** fills that bank, on every machine, with eight new grooves in the key and the
  **STYLE** - acid, techno, electro or deep - laid out as the factory banks are: bass lines
  and leads from the key's scale over a progression of its chords, drums varied from the
  style's factory patterns, and the Mz950 playing a kit's percussion or a tuned programme's
  chords. Every press writes new ones.
- **TRANSPOSE** moves that bank's bass lines and leads into the key, degree for degree - so
  C minor sent to 8B becomes C major, not just a different pitch. A bank written by hand has
  its key worked out from its notes. The drums are left as they are.

## Building

On Windows, with Visual Studio's C++ tools. The repositories go side by side:

    src\
        JUCE\         9.0.2
        Mz101\
        Mz303\
        Mz808\
        Mz909\
        Mz950\        (or VirtualS950, its old name)
        MzPCF\
        MzDebirth\

Mz950's sample disks are not in its repository. Make them once, with its disk generator:

    ..\Mz950\AkaiS950Synth\build.ps1 -To ..\Mz950\disks

then

    .\build.ps1             the sequencer check, then the standalone app
    .\build.ps1 -NoApp      the check only
    .\build.ps1 -Release    and the installer and the zip, into dist\ (needs Inno Setup 6)

The check (`Tests/SeqCheck.cpp`) holds the sequencer to its timing, slides and pattern
changes, checks each effect and every factory groove, generates in all 24 keys, and plays
each machine, before the app is built round it. It links no JUCE. Without the disks it skips
the Mz950's checks and says so; `-Release` will not go on without them.

Pushing a version tag (`git push origin v0.3.0`) builds the installer and zip on GitHub -
disks and all - and attaches them to a draft release, with the notes in
`docs/releases/<tag>.md`.

Any of the other repositories can be somewhere else: pass `-DJUCE_PATH=`, `-DMZ101_PATH=`,
`-DMZ303_PATH=`, `-DMZ808_PATH=`, `-DMZ909_PATH=`, `-DMZ950_PATH=` or `-DMZPCF_PATH=` to
cmake.

## Layout

    Source/Core         the clock, patterns, machines, effects, mixer, keys and grooves - no JUCE
    Source/UI           the panels
    Source/Processor.*  the rack as a JUCE processor, and its saved state
    Tests/SeqCheck.cpp  the check
    installer/          the Inno Setup script, and the zip's README

## Licence

Copyright (C) 2026 Simon Moscrop.

Free software under the GNU AGPL v3, as the synths it plays; see LICENSE.

ReBirth and RB-338 are Propellerhead's; TB-303, SH-101, TR-808 and TR-909 are Roland's; S900
and S950 are Akai's; Mixed In Key is Mixed In Key LLC's. MzDebirth is an independent project
and is not affiliated with any of them.
