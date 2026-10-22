#include "ble_midi.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

namespace {

// Standard BLE-MIDI service / characteristic UUIDs (Apple spec).
constexpr char SVC_UUID[]  = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
constexpr char CHAR_UUID[] = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

NimBLECharacteristic* g_char       = nullptr;
volatile bool         g_connected  = false;
volatile bool         g_enabled    = false;   // radio advertising / usable

// ── incoming clock / transport (written from the NimBLE task) ──
volatile uint32_t g_lastExtClockUs = 0;
volatile uint32_t g_lastExtClockMs = 0;
volatile float    g_extBpm         = 0.0f;
volatile uint8_t  g_pendingXport   = 0;       // 0xFA / 0xFB / 0xFC, consumed by poll

void onExtClockPulse() {
  const uint32_t nowUs = micros();
  const uint32_t prev  = g_lastExtClockUs;
  g_lastExtClockUs = nowUs;
  g_lastExtClockMs = millis();
  if (!prev) return;
  const uint32_t dtUs = nowUs - prev;
  if (dtUs < 1500 || dtUs > 250000) return;   // 10..1000 BPM sanity
  const float inst = 60000000.0f / ((float)dtUs * 24.0f);
  g_extBpm = (g_extBpm > 1.0f) ? (g_extBpm * 0.82f + inst * 0.18f) : inst;
}

// Scan a BLE-MIDI packet for system real-time bytes. In a BLE-MIDI packet
// every message (real-time included) is preceded by a timestamp byte
// (high bit set), so a 0xF8/0xFA/0xFB/0xFC sitting right after a
// high-bit byte is a genuine real-time message.
void parseIncoming(const uint8_t* p, size_t n) {
  if (n < 3) return;
  for (size_t i = 1; i + 1 < n; ++i) {
    if (!(p[i] & 0x80)) continue;
    switch (p[i + 1]) {
      case 0xF8: onExtClockPulse(); break;
      case 0xFA: case 0xFB: case 0xFC: g_pendingXport = p[i + 1]; break;
      default: break;
    }
  }
}

class CharCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    const NimBLEAttValue v = c->getValue();
    parseIncoming(v.data(), v.size());
  }
};
CharCB g_charCB;

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* srv, NimBLEConnInfo& info) override {
    g_connected = true;
    srv->updateConnParams(info.getConnHandle(), 6, 12, 0, 200);  // tight interval for clock
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    g_connected = false;
    if (g_enabled) NimBLEDevice::startAdvertising();
  }
};
ServerCB g_serverCB;

void writePacket(const uint8_t* midi, size_t n) {
  if (!g_enabled || !g_connected || g_char == nullptr || n == 0 || n > 5) return;
  const uint32_t ts = millis();
  uint8_t buf[7];
  buf[0] = 0x80 | ((ts >> 7) & 0x3F);
  buf[1] = 0x80 | (ts & 0x7F);
  for (size_t i = 0; i < n; ++i) buf[2 + i] = midi[i];
  g_char->notify(buf, n + 2);
}

inline uint8_t ch0(uint8_t ch) {
  if (ch < 1)  return 0;
  if (ch > 16) return 15;
  return ch - 1;
}

}  // namespace

BleMidi BLEMIDI;

void BleMidi::begin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(9);
  NimBLEDevice::setSecurityAuth(true, false, true);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&g_serverCB);

  NimBLEService* svc = server->createService(SVC_UUID);
  g_char = svc->createCharacteristic(
      CHAR_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  g_char->setCallbacks(&g_charCB);
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setName(deviceName);
  adv->enableScanResponse(true);
  adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);
  adv->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
  adv->setMinInterval(0x20);
  adv->setMaxInterval(0x40);
  // starts DISABLED — call setEnabled(true) to advertise
}

void BleMidi::setEnabled(bool on) {
  if (on == g_enabled) return;
  g_enabled = on;
  if (on) {
    NimBLEDevice::startAdvertising();
  } else {
    NimBLEServer* srv = NimBLEDevice::getServer();
    if (srv) for (auto h : srv->getPeerDevices()) srv->disconnect(h);
    NimBLEDevice::stopAdvertising();
    g_lastExtClockMs = 0;
    g_extBpm = 0.0f;
  }
}

bool BleMidi::enabled() const    { return g_enabled; }
bool BleMidi::connected() const  { return g_enabled && g_connected; }
bool BleMidi::advertising() const { return g_enabled && !g_connected; }

bool  BleMidi::extClockActive() {
  return g_lastExtClockMs != 0 && (millis() - g_lastExtClockMs) < 600;
}
float BleMidi::extBpm() { return g_extBpm; }
int   BleMidi::pollTransport() {
  const int t = g_pendingXport;
  g_pendingXport = 0;
  return t;
}

void BleMidi::send1(uint8_t status) { writePacket(&status, 1); }
void BleMidi::send2(uint8_t status, uint8_t d1) {
  const uint8_t m[2] = {status, (uint8_t)(d1 & 0x7F)};
  writePacket(m, 2);
}
void BleMidi::send3(uint8_t status, uint8_t d1, uint8_t d2) {
  const uint8_t m[3] = {status, (uint8_t)(d1 & 0x7F), (uint8_t)(d2 & 0x7F)};
  writePacket(m, 3);
}

void BleMidi::noteOn(uint8_t ch, uint8_t note, uint8_t vel) { send3(0x90 | ch0(ch), note, vel); }
void BleMidi::noteOff(uint8_t ch, uint8_t note)             { send3(0x80 | ch0(ch), note, 0); }
void BleMidi::programChange(uint8_t ch, uint8_t program)    { send2(0xC0 | ch0(ch), program); }
void BleMidi::controlChange(uint8_t ch, uint8_t cc, uint8_t value) { send3(0xB0 | ch0(ch), cc, value); }
void BleMidi::allNotesOff(uint8_t ch)                       { send3(0xB0 | ch0(ch), 123, 0); }

void BleMidi::clock()             { send1(0xF8); }
void BleMidi::transportStart()    { send1(0xFA); }
void BleMidi::transportContinue() { send1(0xFB); }
void BleMidi::transportStop()     { send1(0xFC); }
