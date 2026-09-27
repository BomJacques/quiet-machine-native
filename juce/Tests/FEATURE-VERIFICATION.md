# Feature Verification Handoff

Status: integrated Windows x64 Release build and tests passed on 2026-09-27.
Version 0.2.0. All 15 processor/catalogue groups passed. Both snapshot runs
passed host recall, 48 bindings, timed chord capture, help interactions and
all 24 preset selections/audio renders. Screenshots were produced at 1120x760,
1080x760 and (help overlay) 1440x760. These are native component tests,
not Reason host or physical keyboard validation.

Changes are confined to `juce/Tests`: existing runner extended, shared feature
checks in `FeatureTests.h`, and usage/contracts documented in `README.md`.
No CMake, processor, editor or factory source was edited by the test owner.

Run the integrated Release executable in separate processes:

```text
QuietMachineSmoke.exe
QuietMachineSmoke.exe --snapshot editor.png
QuietMachineSmoke.exe --snapshot editor-help.png --help-overlay
```

The first command preserves all 13 existing regressions and adds raw held-input
and 24-preset catalogue checks. Each snapshot run preserves open-editor host
recall and exactly 48 parameter bindings, then runs actual timed chord capture,
keyboard callback/help interactions and all 24 preset UI selections/renders.
Each writes normal and minimum-size PNGs. `--ui-tests` runs the same interactions
without image output. Exit 1 indicates a failed assertion or invalid arguments.

Each GUI invocation uses exactly one JUCE dispatch loop, including preset renders
and any requested final help toggle. Synthetic component callbacks and offscreen
hit-testing need no browser, desktop automation, audio device or desktop peer.

Timing assertions intentionally check early non-commit at 200/300 ms and allow
750 ms overall after a chord change for 600 ms stability plus 24 Hz polling.
Capture completion is verified against processor JSON, not visual status text.
