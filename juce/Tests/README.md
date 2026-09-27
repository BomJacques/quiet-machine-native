# Native Verification

Only this fork's tests are replaced. The original VX smoke test is untouched.
Uses the existing QuietMachineSmoke target; no additional target is required.

Run the Release executable:

```text
QuietMachineSmoke.exe
QuietMachineSmoke.exe --benchmark
QuietMachineSmoke.exe --ui-tests
QuietMachineSmoke.exe --snapshot editor.png
QuietMachineSmoke.exe --snapshot editor-wide.png --size 1400x900
QuietMachineSmoke.exe --snapshot editor-help.png --help-overlay
```

No options runs all existing DSP regressions plus raw-input and factory-catalogue
contracts (independent failures are reported). `--ui-tests` runs the targeted native
GUI sequence without writing PNGs. Snapshot mode runs that same GUI sequence.
Benchmark, GUI and snapshot modes skip the full DSP regressions and may be combined.
Most DSP renders use 48 kHz / 512 samples. A compact callback matrix also uses
44.1/48/96 kHz with changing 0/64/127/512/2048-sample callbacks and notes at
the last valid sample offset of each nonempty size. Benchmark prints wall seconds,
rendered audio seconds, and wall/audio ratios for default-patch idle and
four-note HOLD activity, after warmup. It opens no audio device and imposes
no machine-dependent performance threshold.

Snapshot mode creates the actual native editor on the JUCE message thread and
paints its component tree offscreen. A single timer-driven message loop performs
host recall, GUI interactions and capture waits; it stops only after all stages,
avoiding JUCE's sticky dispatch-loop quit flag. During dispatch it recalls host
state through setStateInformation while the
editor and parameter attachments remain alive, then verifies restored state
and silence before capturing. This is a message-thread recall smoke, not a
concurrent audio/UI stress test.
It writes a PNG at normal/requested size plus a sibling `*-min.png` at the
editor constrainer's minimum bounds when defined. It checks nonblank pixels
and decodes the written PNG to verify dimensions. No desktop peer, browser,
network resource, or screen capture is used. `--help-overlay` leaves help visible
in both PNGs. Inspect both PNGs visually for layout and text overlap.

## New Feature Checks

- Raw held-input bits are MIDI 48..83 minus 48, with bit 63 marking any outside
  note. Tests exercise NOTES/chord expansion, boundaries, partial/last releases,
  all-notes/all-sound-off, generated HOLD exclusion, panic and restore (including
  pending requests), deferred zero-sample events, mode changes, and host/UI
  same-pitch overlap with independent releases, including host channels 1, 2
  and 16 and channel-scoped all-notes/all-sound-off.
- All 48 parameter bindings remain checked. GUI-only IDs have an explicit
  allowlist; unrecognised IDs still fail. Latch/Audition IDs and default, native
  keyboard toggle persistence, selected slot isolation and note-name labels
  are exercised through public JUCE component methods and a synthetic event.
- The actual editor timer captures raw MIDI in chord mode. Capture ignores a
  pre-held chord, waits through release/replay, does not apply at 200 ms, resets
  stability on added notes, commits after at least 600 ms, then disarms. Further
  input cannot apply twice. Empty and out-of-range chords leave slots unchanged;
  valid capture into both MIDI and HOLD slots is verified. Waits allow 750 ms
  for the final 600 ms threshold and 24 Hz polling; no mock timer is used.
- Help is nonopaque, intercepts pointer hit-testing over editing controls, and
  closes through Escape or its asynchronous Close button. The test does not
  bypass the overlay by directly invoking covered controls.
- Factory catalogue contains 24 unique names and sound/chord signatures, retains
  the first four names/order, and has valid parameter IDs, values and note ranges.
  Every UI selection applies both chords and parameter overrides, stops HOLD,
  and is silent after consuming its panic/reset. With Static set to zero, each
  preset then renders 188 blocks (2.005 s at 48 kHz), requiring finite, nonzero
  energy and peak <= 1.05. Peak/energy are printed per preset. These bounds are
  smoke checks, not subjective sound-quality or universal clipping guarantees.

## Contracts

- Startup and restored state remain silent, including Static/Hiss.
- The variable-callback matrix checks finite audio, audible onset, complete
  release silence, and a reported tail length of 20 seconds.
- HOLD emits exactly notes 48, 52, 55 for autoNotes [0,4,7], channel 16,
  and matching note-offs when disabled. No later MIDI retriggers, including
  across former beat boundaries and a tempo change.
- engineMode, navigationMode, pianoSequencerEnabled, pianoRateHz, and
  followHostTransport are absent; HOLD is manual only.
  Legacy patch program/navigation fields do not override autoNotes.
- Zero-sample callbacks cannot consume or emit the initial HOLD gate.
  Physical note-on/off delivered in empty callbacks survive into real audio
  callbacks, even when the host discards its MIDI buffer in between.
- Exporting MIDI while HOLD is still active preserves the full rendered
  duration (just over one second), with one on/off per chord note. A second
  export after more rendering extends each note without duplicating events.
- A short master release followed by more than seven seconds of offline idle
  cannot resurrect old piano voices on a disjoint chord. Its isolated piano
  stem must match a fresh processor playing that chord, with audible
  preconditions for both old and new notes.
- MIDI chord shape is transposed relative to its minimum note; shared notes
  survive overlapping root release; changing shape while held cannot strand
  expanded notes. Drone fundamentals are checked with a windowed projection.
- Whole-machine attack, decay/sustain, release, interrupted-attack release,
  and immediate panic include Static/Hiss and the processed master.
- Delay-on leaves an echo when source input is removed during master release;
  delay-off does not. State retains ADSR, delay/chord switches, chord arrays,
  and complete key-voice JSON while clearing transport/record switches.

## Remaining Gaps

No host transport simulation, exhaustive block sizes/sample rates, realtime
deadline guarantee, MIDI queue saturation, concurrent state changes, or
long-duration soak. Key-voice persistence is checked, not all detune/pan/
oscillator rendering combinations from the original extensive VX suite.
Piano legacy controls are absent, but no isolated piano-envelope regression
proves that internal periodic piano triggering has been removed.
Component callback tests do not cover OS input delivery, typing/focus integration,
accessibility, host embedding, or layout overlap. The Audition callback return
contract is checked; full native pointer/drag playback is not simulated.
Test failures should not be hidden by
relaxing assertions without checking processor behavior.
