// ClockFollower - prints incoming MIDI clock tempo and transport
// (Start/Continue/Stop) sent by the connected host (DAW, Norns, OP-XY...).
#include <Arduino.h>
#include "ble_midi.h"

void setup() {
  Serial.begin(115200);
  BLEMIDI.begin("Core2MIDI");
  BLEMIDI.setEnabled(true);
}

void loop() {
  switch (BLEMIDI.pollTransport()) {          // returns each event once
    case 0xFA: Serial.println("START");    break;
    case 0xFB: Serial.println("CONTINUE"); break;
    case 0xFC: Serial.println("STOP");     break;
  }
  static uint32_t last = 0;
  if (millis() - last > 1000) {
    last = millis();
    if (BLEMIDI.extClockActive()) Serial.printf("clock: %.1f BPM\n", BLEMIDI.extBpm());
    else Serial.println(BLEMIDI.connected() ? "connected, no clock" : "advertising...");
  }
}
