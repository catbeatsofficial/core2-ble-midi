// ble_midi.h - minimal BLE-MIDI 1.0 peripheral.
//
// Sends notes / program changes / (when master) clock + transport, and
// parses INCOMING system-real-time so the cat can follow an external
// clock. Radio can be toggled off entirely (setEnabled).
#pragma once
#include <cstddef>
#include <cstdint>

class BleMidi {
 public:
  void begin(const char* deviceName);   // inits the stack; starts DISABLED

  // radio control
  void setEnabled(bool on);             // on = advertise/pair; off = stop + drop
  bool enabled() const;
  bool connected() const;
  bool advertising() const;             // enabled, discoverable, not yet linked

  // outgoing
  void noteOn(uint8_t ch, uint8_t note, uint8_t vel);   // ch = 1..16
  void noteOff(uint8_t ch, uint8_t note);
  void programChange(uint8_t ch, uint8_t program);
  void controlChange(uint8_t ch, uint8_t cc, uint8_t value);
  void allNotesOff(uint8_t ch);
  void clock();             // 0xF8
  void transportStart();    // 0xFA
  void transportContinue(); // 0xFB
  void transportStop();     // 0xFC

  // incoming (polled from loop())
  bool  extClockActive();   // external 0xF8 seen within the last ~0.6 s
  float extBpm();           // smoothed BPM derived from that clock
  int   pollTransport();    // returns 0xFA/0xFB/0xFC once, then 0

 private:
  void send1(uint8_t status);
  void send2(uint8_t status, uint8_t d1);
  void send3(uint8_t status, uint8_t d1, uint8_t d2);
};

extern BleMidi BLEMIDI;
