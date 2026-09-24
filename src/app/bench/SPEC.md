# App Bench (M1) — Viewer Benchmark Mode

Status: story 1.5. The bench mode drives the viewer loop headless and emits
the frame_ms and peak_rss_kib numbers `tools/bench-measure.sh` needs to
verify the R15.1 frame budget and the R30.2 resident-memory ceiling. It is a
selftest knob like TYNYPDF_FRAMES: the interactive product never reads it.

## Requirements

### R31.1 The bench mode SHALL drive the viewer loop headless, emit forward and return pass frame_ms and peak_rss_kib metrics as JSON, and terminate once the frame budget is exhausted, so tools/bench-measure.sh measures R15.1 and R30.2.

Verification: script:tools/bench-measure.sh

## Out of scope

- The R15.1 and R30.2 measurements themselves (they belong to the harness)
- Cold start, DPI and gesture timing (story 5.4, R24.4-R24.6)
- Windows-specific present details (the headless loop honours TYNYPDF_FRAMES)
