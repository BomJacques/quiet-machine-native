# Quiet Machine Native

A separate native JUCE fork of Quiet Machine VX. The original project, HTML UI,
builds, and installation are not modified. This plugin uses its own VST3 identity
and appears as **Quiet Machine Native** alongside Quiet Machine VX.

## What Changed

- Native JUCE controls: no embedded browser, React, runtime CSS, CDN, or WebView2.
- Random chord navigation, timed chord programs, and piano step sequencing removed.
- HOLD sustains the chosen chord without periodically re-triggering it. MIDI input
  takes priority; MIDI chord mode transposes the selected shape from the played note.
- The twelve sound sources, tape degradation, master processing, whole-output ADSR,
  per-key voice profiles, and optional source output buses remain.
- Silent processing sleeps only after the master release has completed. Old source
  tails are cleared at that point so they cannot return on a later note.
- Cached DSP parameter pointers, unchanged filter/profile recalculation avoidance,
  occupied piano-voice scans, event-driven gate checks, and vectorized mixing.

## Install on Windows

1. Close Reason and any other application using the plugin.
2. Extract the plugin-only `QM.zip` directly to `C:\QM` to avoid long Windows paths.
3. Copy the **entire** `Quiet Machine Native.vst3` folder into
   `C:\Program Files\Common Files\VST3`. Administrator approval may be required.
4. Open Reason and insert **Quiet Machine Native** as a new instrument.

Do not replace the original Quiet Machine VX bundle. Existing VX instances will
continue to use VX; this fork does not silently replace instruments in old songs.
The optional standalone EXE is for auditioning, not for installation as a VST.
This build is Windows x64 and imports the Microsoft Visual C++ v14 x64 runtime
(`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`). That runtime must also
be present on another machine. No browser runtime is needed.

## Native Controls

- Use HOLD next to Stop for continuous playback; Stop also clears active voices.
- Latch is the default keyboard mode: click a key to add or remove it immediately.
  Note names appear above the keyboard and on the keys. There is no Apply button.
  Audition plays without changing the saved chord.
- Arm Hold chord to apply, then play/replay MIDI or typing-keyboard notes and hold
  them steady for 600 ms. It captures once into the selected chord slot. Use C3-B5,
  with a maximum of 16 notes; invalid chords leave the saved shape unchanged.
  Notes held before arming must change or be replayed before capture can apply.
  Editing or capture does not switch on the main HOLD playback control.
- HOLD chord and MIDI chord are separate slots. To MIDI / To HOLD replaces the
  other slot only after confirmation.
- MIDI chord expands a host MIDI note into the selected shape, rooted at that
  played note. The key-voice section edits Drone/Sub profiles, not every texture.
- MIDI chord per note is the large ON/OFF button beside the chord editor.
  Clicking ON selects MIDI shape for editing; it does not enable HOLD.
- Drag dials vertically, double-click to reset, or type a value in its readout.
- Overall ADSR Legato ON keeps the envelope continuous over overlapping MIDI
  notes. OFF retriggers Attack on each incoming note from its current level,
  avoiding an abrupt reset to silence. Release still starts after the last note.
  This is a global envelope, not independent per-note envelopes or pitch glide.
  Legato defaults ON to preserve earlier behavior and is saved in plugin state.
  Factory sound selection leaves this performance preference unchanged.
- The preset bank contains 24 sounds: the original four plus 20 additional ambient
  patches. Save/Load retains your complete custom sound and chord profiles.
- Press ? for the translucent control guide. Select a highlighted section or a
  guide tab to explain its controls. Close or Escape returns to normal operation.
- The editor is 760 logical pixels tall and 1080-1440 pixels wide; the keyboard
  and control heights stay bounded instead of stretching when resized.

## Audio and MIDI

The first stereo output is the fully processed master. Additional source buses
are separate source signals with the global envelope, without master effects.
Use the processed master when you want tape, delay, reverb, and freeze printed.
The plugin cannot create or route Reason sequencer tracks automatically.

Typing or clicking inside a plugin is direct auditioning, not host MIDI recording.
Host-routed MIDI remains the route for recording performances in Reason. The
native editor removes browser focus handling, but does not override Reason's
typing-keyboard routing rules.

## Build

Requires a JUCE 8 checkout, Visual Studio with Desktop development with C++, and
CMake/Ninja. Run in a Visual Studio x64 developer terminal from `juce`:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=C:/SDK/JUCE
cmake --build build --target QuietMachineVX_All QuietMachineSmoke --parallel 4
.\build\QuietMachineSmoke_artefacts\Release\QuietMachineSmoke.exe
```

The internal CMake target remains `QuietMachineVX` to keep source integration
small; the product name, bundle ID, and four-character plugin code are distinct.
No WebView2 SDK or runtime is required for this fork.

## Verification Limits

Automated native processor tests and native component screenshots are development
checks, not a substitute for host testing. Confirm your own Reason project,
automation, sample rate, audio device buffer size, and long sessions before relying
on this initial fork in production. Third-party JUCE licensing obligations still
apply to distribution.
