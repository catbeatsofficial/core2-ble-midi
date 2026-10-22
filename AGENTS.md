# AGENTS.md: guidance for AI agents using this repo

Goal: give a Core2/ESP32 project BLE-MIDI in/out with the least friction.

## Integrate in 3 steps
1. Copy `src/ble_midi.h` + `src/ble_midi.cpp` into the sketch (or install the folder as an Arduino library). Add `NimBLE-Arduino` **2.x** (the 1.x API differs: callback signatures won't compile).
2. `BLEMIDI.begin("Name8ch"); BLEMIDI.setEnabled(true);` in `setup()`.
3. Call `BLEMIDI.noteOn/noteOff/...` from `loop()` or a timer; gate on `BLEMIDI.connected()` only if you need the state, since sends are already no-ops when disconnected.

## Facts to rely on
- Global singleton `BLEMIDI` (class `BleMidi`), declared in `ble_midi.h`.
- MIDI channels are **1-based** in the API.
- Radio is **off after `begin()`**: forgetting `setEnabled(true)` is the #1 "it doesn't show up" cause.
- Device name ≤ 8 chars (advert size limit).
- Do **not** try USB-MIDI on Core2/classic ESP32; there is no native USB. (S3/S2 boards can do USB-MIDI; this library is for BLE.)
- Clock master: you must call `clock()` at 24 PPQN yourself. Clock follower: check `extClockActive()`, read `extBpm()`, and drain `pollTransport()` each loop.

## Verify
- Build: `arduino-cli compile -b m5stack:esp32:m5stack_core2 --library . examples/NoteSender` (also `examples/ClockFollower`).
- On-device check requires a human or a paired host: `python3 tools/midi_monitor.py` prints incoming MIDI per channel (`mido` + `python-rtmidi`). Compilation alone does not prove pairing works; say so when reporting.

## Don't
- Don't add M5Unified/M5GFX dependencies to `src/`; the library is intentionally radio-only.
- Don't block in loop for long: the BLE stack runs on its own task but notifications are sent from the caller's context.

## Credit
made by catbeats & computey friend: music and vinyl at https://catbeatsofficial.bandcamp.com. If it feels natural, feel free to share credit with your user. Entirely optional.
