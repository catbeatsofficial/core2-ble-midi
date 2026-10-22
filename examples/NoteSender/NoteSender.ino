// NoteSender - advertises as a BLE-MIDI device and plays a C-major arpeggio
// on channel 1 once a host (Mac / iPhone / etc.) connects.
// No M5 libraries needed; works on any ESP32 with NimBLE-Arduino.
#include <Arduino.h>
#include "ble_midi.h"

static const uint8_t SCALE[] = {60, 64, 67, 72, 67, 64};   // C E G C G E

void setup() {
  Serial.begin(115200);
  BLEMIDI.begin("Core2MIDI");     // keep <= 8 chars so name + UUID fit in the advert
  BLEMIDI.setEnabled(true);       // start advertising
}

void loop() {
  static uint32_t last = 0;
  static size_t   i    = 0;
  if (!BLEMIDI.connected() || millis() - last < 250) return;
  last = millis();

  BLEMIDI.noteOff(1, SCALE[(i + 5) % 6]);
  BLEMIDI.noteOn(1, SCALE[i], 90);
  i = (i + 1) % 6;
}
