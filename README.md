# MzDebirth

A rack of the Mz synths, played from a step sequencer, in the spirit of the old ReBirth
RB-338: each machine with its own panel, its own patterns and a channel on the mixer, all on
one clock.

It plays the synths' real engines. Nothing is copied: each engine is compiled straight from
its own repository, checked out beside this one, so a fix to a synth reaches the rack too.

The rack, top to bottom:

- **Mz303** - the full panel, and a piano-roll step editor with the 303's UP, DOWN, ACCENT
  and SLIDE on every step. A slide holds the note over into the next step and glides to it,
  as the engine does when one note is played over another.
- **Mz101** - the panel in its sections, less what the rack does for it (the arpeggiator,
  sequencer and clock). The same step editor without ACCENT, which the 101 has none of; its
  SLIDE is a legato, which glides when PORTAMENTO is on AUTO. Its modulator's SYNC can lock
  to the rack's tempo.
- **Mz808** and **Mz909** - the full panels, in a column per instrument, and the hardware's
  way of writing a beat: pick an instrument (or AC for the accents) and its steps show on the
  sixteen keys.
- **Mz950** - plays S900/S950 disk images: LOAD DISK, then pick a programme. Its steps are a
  grid of notes, any number on a step, with ACCENT and TIE rows; a tied step carries on the
  notes it shares with the step before. The keys the programme answers are lit, and a drum
  kit's are named by their samples. The knobs are the player's trims over every keygroup.
  It opens with the drum disk that comes with Mz950.

And for all of them:

- **Patterns** - four banks of eight per machine, up to sixteen steps each. A pattern picked
  while playing waits for the one playing to come round, and blinks until it does.
- **Transport** - tempo, a shuffle that each machine can follow or not, and a master level.
  The space bar plays and stops.
- **Mixer** - level, pan and mute for each machine, its switches into the DIST, PCF and
  COMP, and its send to the DELAY.
- **Effects** - ReBirth's four units, along under the transport. **DIST**: drive and a shape
  from soft to hard to a lopsided fuzz. **PCF**: a filter played by one of 32 step patterns,
  struck on every sixteenth and closing again by DEC, low-pass or band-pass. **DELAY**: echoes
  1 to 16 sixteenths behind at the rack's tempo (or triplets), with feedback and pan; they
  ring on after STOP. **COMP**: ratio and threshold, for the channels switched into it, and
  for the whole mix with MASTER. A channel goes through DIST, then PCF, then COMP, then its
  fader; each channel has its own of each, sharing the unit's knobs.
- **Files** - the whole rack, patterns and all, saved to and loaded from `.mzdebirth` files.
  The sampler's disk is saved by where it is, not copied in. The standalone app also
  remembers where it was left.

## Building

On Windows, with Visual Studio's C++ tools. The repositories go side by side:

    src\
        JUCE\
        Mz101\
        Mz303\
        Mz808\
        Mz909\
        Mz950\        (or VirtualS950, its old name)
        MzDebirth\

then

    .\build.ps1             the sequencer check, then the standalone app
    .\build.ps1 -NoApp      the check only
    .\build.ps1 -Release    and the installer and a zip, into dist\ (needs Inno Setup 6)

The installer puts the app in Program Files with the six sample disks that come with
Mz950 beside it, in `disks\`; the Mz950 opens its drums from there. Pushing a version tag
(`git push origin v0.2.0`) builds both on GitHub and attaches them to a draft release.

The check (`Tests/SeqCheck.cpp`) holds the sequencer to its timing, slides, pattern changes
and output level, and plays each machine, before the app is built round it. It links no
JUCE.

Any of the other repositories can be somewhere else: pass `-DJUCE_PATH=`, `-DMZ101_PATH=`,
`-DMZ303_PATH=`, `-DMZ808_PATH=`, `-DMZ909_PATH=` or `-DMZ950_PATH=` to cmake.

## Layout

    Source/Core         the clock, the patterns, the machines and the mixer - no JUCE
    Source/UI           the panels
    Source/Processor.*  the rack as a JUCE processor, and its saved state

## Licence

Copyright (C) 2026 Simon Moscrop.

Free software under the GNU AGPL v3, as the synths it plays; see LICENSE.

ReBirth and RB-338 are Propellerhead's; TB-303, SH-101, TR-808 and TR-909 are Roland's; S900
and S950 are Akai's. MzDebirth is an independent project and is not affiliated with any of
them.
