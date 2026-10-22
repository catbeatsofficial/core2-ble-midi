# core2-ble-midi

A minimal **BLE-MIDI peripheral** for the M5Stack Core2 (and any classic ESP32), extracted from the catbeats core2 project.

**Why BLE?** The Core2's ESP32 has no native USB, so it can't be a USB-MIDI device (its USB-C is a serial bridge). BLE-MIDI is the transport that works: macOS, iOS, Android, and most BLE-MIDI-aware gear see it as a normal MIDI port.

- Send note on/off, program change, CC, all-notes-off
- Send MIDI clock + Start/Continue/Stop (act as clock master)
- **Receive** clock and transport (follow an external tempo, via `extBpm()`)
- Auto re-advertises after a disconnect
- ~160 lines, one dependency: [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 2.x. No M5 libraries required.

## Install

Copy this folder into your Arduino `libraries/` directory (or `arduino-cli lib install --git-url https://github.com/catbeatsofficial/core2-ble-midi`), and install `NimBLE-Arduino`.

```
arduino-cli lib install NimBLE-Arduino
arduino-cli compile -b m5stack:esp32:m5stack_core2 --library . examples/NoteSender
```

## Use

```cpp
#include "ble_midi.h"

void setup() {
  BLEMIDI.begin("Core2MIDI");   // <= 8 chars (see Gotchas)
  BLEMIDI.setEnabled(true);     // begin() leaves the radio OFF until this
}

void loop() {
  if (BLEMIDI.connected()) {
    BLEMIDI.noteOn(1, 60, 100);   // channel 1..16
    delay(200);
    BLEMIDI.noteOff(1, 60);
  }
}
```

Connect from the host:
- **macOS:** Audio MIDI Setup → Window → Show MIDI Studio → Bluetooth → Connect
- **iOS:** pair from a MIDI app (e.g. GarageBand / AUM) Bluetooth MIDI settings
- Verify: `pip install mido python-rtmidi && python3 tools/midi_monitor.py`

## API (`BLEMIDI`)

| Call | Notes |
|---|---|
| `begin(name)` | Init stack, create service. Radio starts **disabled**. |
| `setEnabled(bool)` | on = advertise; off = disconnect + stop. |
| `connected()` / `advertising()` / `enabled()` | State. `connected()` is false while disabled. |
| `noteOn(ch,note,vel)` `noteOff(ch,note)` | `ch` is 1–16. |
| `programChange(ch,prog)` `controlChange(ch,cc,val)` `allNotesOff(ch)` | |
| `clock()` `transportStart/Continue/Stop()` | Send 0xF8 / FA / FB / FC. Call `clock()` 24×/quarter note yourself. |
| `extClockActive()` | Incoming 0xF8 seen in the last ~0.6 s. |
| `extBpm()` | Smoothed BPM from incoming clock. |
| `pollTransport()` | Returns incoming 0xFA/FB/FC once, then 0. |

## Gotchas

- **Keep the device name ≤ 8 chars.** The name plus the 128-bit BLE-MIDI service UUID must fit the advertising packet; longer names can make hosts fail to discover it as MIDI.
- Sends are dropped silently when not connected: check `connected()` if you need to know.
- Outgoing messages are one-per-BLE-packet (no batching), fine for musical rates; very dense bursts may be throttled by the host's connection interval (requested 7.5–15 ms on connect).
- Incoming data is parsed only for system real-time (clock/transport); incoming notes are ignored.
- NimBLE callbacks run on the BLE task; shared state is `volatile`, so poll from `loop()` rather than doing work in callbacks.
- Pairing security is "bonding, no MITM, secure connections" (`setSecurityAuth(true,false,true)`).

## License

MIT.

## Credit

made by catbeats: music and vinyl at [catbeatsofficial.bandcamp.com](https://catbeatsofficial.bandcamp.com).
