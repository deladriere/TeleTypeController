/*
 * Teletype MIDI Controller v1.0.1
 * USB MIDI (notes/CC/pitch bend + SysEx config) and Serial MIDI TX0 (performance)
 * Configuration stored in RP2040 emulated EEPROM (flash)
 */

#define PICO_VERSION "v1.0.1"

#include "config_eeprom.h"
#include "sysex_protocol.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>
#include <MIDI.h>

// Forward declarations
uint16_t calculateChecksum();
void resetConfigToDefaults();
void saveConfig();
bool loadConfig();
void initInputPins();
int readBank();
void sendMomentaryNote(byte note, uint32_t color);
void handleSysEx(byte *array, unsigned size);
void sendSysEx(byte cmd, const byte *payload, unsigned len);
void sendAck(byte cmd, byte status);
void sendVersion();
void sendConfigDump();

// ── Buttons: pins 2-8 ────────────────────────────────────────────────────────
#define BUTTON_PIN1 2
#define BUTTON_PIN2 3
#define BUTTON_PIN3 4
#define BUTTON_PIN4 5
#define BUTTON_PIN5 6

#define BANK_SEL_PIN0 8  // Bit 0
#define BANK_SEL_PIN1 7  // Bit 1

#define LED_PIN 16
#define LED_COUNT 1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long ledOnTime = 0;
bool ledActive = false;
const unsigned long LED_FLASH_MS = 100;

const uint32_t BANK_COLORS[NUM_BANKS] = {
    0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF
};

#define EEPROM_SIZE 512

// USB MIDI
Adafruit_USBD_MIDI usbd_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbd_midi, USB_MIDI);

// Hardware Serial MIDI (TX0 = GPIO0) — performance only, not config
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, SERIAL_MIDI);

bool lastRawReading1 = HIGH;
bool lastRawReading2 = HIGH;
bool lastRawReading3 = HIGH;
bool lastRawReading4 = HIGH;
bool lastRawReading5 = HIGH;
bool lastStableState1 = HIGH;
bool lastStableState2 = HIGH;
bool lastStableState3 = HIGH;
bool lastStableState4 = HIGH;
bool lastStableState5 = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
unsigned long lastDebounceTime5 = 0;
const unsigned long DEBOUNCE_DELAY = 50;

const int POT_PINS[MAX_POTS] = {A0, A1, A2, A3};
int potSmoothed[MAX_POTS] = {0, 0, 0, 0};
int lastMidiValue[MAX_POTS] = {-1, -1, -1, -1};
const float POT_ALPHA = 0.15;

struct Config {
  byte btnChannel;
  byte potChannel;
  byte buttonNotes[MAX_BUTTONS];
  byte potTypes[MAX_POTS];
  byte potValues[MAX_POTS];
  byte potFlags;
} config;

byte clampMidiChannel(byte ch, byte fallback) {
  return (ch >= 1 && ch <= 16) ? ch : fallback;
}

bool isPotIgnored(int potIndex) {
  return (config.potFlags & (1 << potIndex)) != 0;
}

void setPotIgnored(int potIndex, bool ignored) {
  if (ignored) {
    config.potFlags |= (1 << potIndex);
    lastMidiValue[potIndex] = -1;
  } else {
    config.potFlags &= ~(1 << potIndex);
    potSmoothed[potIndex] = analogRead(POT_PINS[potIndex]);
    lastMidiValue[potIndex] = -1;
  }
}

int readBank() {
  int bit0 = (digitalRead(BANK_SEL_PIN0) == HIGH) ? 1 : 0;
  int bit1 = (digitalRead(BANK_SEL_PIN1) == HIGH) ? 1 : 0;
  return bit0 | (bit1 << 1);
}

void setup() {
  for (int p = 2; p <= 8; p++) {
    pinMode(p, INPUT_PULLUP);
  }

  led.begin();
  led.setBrightness(30);
  led.clear();
  led.show();

  for (int i = 0; i < MAX_POTS; i++) {
    analogRead(POT_PINS[i]);
    delayMicroseconds(50);
    potSmoothed[i] = analogRead(POT_PINS[i]);
  }

  // Arduino-Pico initializes TinyUSB and its CDC upload port before setup().
  // Initializing it again clears CDC from the USB configuration. Device names
  // are supplied by platformio.ini; only the MIDI cable name is set here.
  usbd_midi.setCableName(1, "Teletype");

  USB_MIDI.setHandleSystemExclusive(handleSysEx);
  USB_MIDI.begin(MIDI_CHANNEL_OMNI);

  // Let the host discover MIDI too if CDC was already enumerated during boot.
  // This follows the installed TinyUSB MIDI example without rebuilding CDC.
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  delay(100);

  Serial1.setTX(0);
  SERIAL_MIDI.begin(MIDI_CHANNEL_OMNI);

  EEPROM.begin(EEPROM_SIZE);

  if (!loadConfig()) {
    resetConfigToDefaults();
  }

  initInputPins();
}

void initInputPins() {
  unsigned long now = millis();
  lastRawReading1 = digitalRead(BUTTON_PIN1);
  lastRawReading2 = digitalRead(BUTTON_PIN2);
  lastRawReading3 = digitalRead(BUTTON_PIN3);
  lastRawReading4 = digitalRead(BUTTON_PIN4);
  lastRawReading5 = digitalRead(BUTTON_PIN5);
  lastStableState1 = lastRawReading1;
  lastStableState2 = lastRawReading2;
  lastStableState3 = lastRawReading3;
  lastStableState4 = lastRawReading4;
  lastStableState5 = lastRawReading5;
  lastDebounceTime1 = now;
  lastDebounceTime2 = now;
  lastDebounceTime3 = now;
  lastDebounceTime4 = now;
  lastDebounceTime5 = now;
}

uint16_t calculateChecksum() {
  uint16_t sum = 0;
  for (uint16_t addr = 0; addr < EEPROM_ADDR_CHECKSUM; addr++) {
    sum += EEPROM.read(addr);
  }
  return sum;
}

void resetConfigToDefaults() {
  config.btnChannel = DEFAULT_BTN_MIDI_CHANNEL;
  config.potChannel = DEFAULT_POT_MIDI_CHANNEL;
  for (int i = 0; i < MAX_BUTTONS; i++) {
    config.buttonNotes[i] = DEFAULT_BUTTON_NOTES[i];
  }
  for (int i = 0; i < MAX_POTS; i++) {
    config.potTypes[i] = DEFAULT_POT_TYPES[i];
    config.potValues[i] = DEFAULT_POT_VALUES[i];
  }
  config.potFlags = 0;
}

void saveConfig() {
  EEPROM.write(EEPROM_ADDR_MAGIC + 0, CONFIG_MAGIC_0);
  EEPROM.write(EEPROM_ADDR_MAGIC + 1, CONFIG_MAGIC_1);
  EEPROM.write(EEPROM_ADDR_MAGIC + 2, CONFIG_MAGIC_2);
  EEPROM.write(EEPROM_ADDR_MAGIC + 3, CONFIG_MAGIC_3);
  EEPROM.write(EEPROM_ADDR_VERSION, CONFIG_VERSION);
  EEPROM.write(EEPROM_ADDR_FLAGS, config.potFlags & POT_FLAGS_IGNORE_MASK);
  EEPROM.write(EEPROM_ADDR_NUM_BTNS, MAX_BUTTONS);
  EEPROM.write(EEPROM_ADDR_NUM_POTS, MAX_POTS);
  EEPROM.write(0x0008, 0);
  EEPROM.write(EEPROM_ADDR_BTN_CH, config.btnChannel);
  EEPROM.write(EEPROM_ADDR_POT_CH, config.potChannel);

  for (int i = 0; i < MAX_BUTTONS; i++) {
    EEPROM.write(EEPROM_ADDR_BUTTONS + i, config.buttonNotes[i]);
  }
  for (int i = 0; i < MAX_POTS; i++) {
    EEPROM.write(EEPROM_ADDR_POTS + (i * 2), config.potTypes[i]);
    EEPROM.write(EEPROM_ADDR_POTS + (i * 2) + 1, config.potValues[i]);
  }
  for (uint16_t addr = 0x0030; addr < EEPROM_ADDR_CHECKSUM; addr++) {
    EEPROM.write(addr, 0);
  }

  uint16_t checksum = calculateChecksum();
  EEPROM.write(EEPROM_ADDR_CHECKSUM, (byte)(checksum >> 8));
  EEPROM.write(EEPROM_ADDR_CHECKSUM + 1, (byte)(checksum & 0xFF));
  EEPROM.commit();
}

bool loadConfig() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC + 0) != CONFIG_MAGIC_0 ||
      EEPROM.read(EEPROM_ADDR_MAGIC + 1) != CONFIG_MAGIC_1 ||
      EEPROM.read(EEPROM_ADDR_MAGIC + 2) != CONFIG_MAGIC_2 ||
      EEPROM.read(EEPROM_ADDR_MAGIC + 3) != CONFIG_MAGIC_3) {
    return false;
  }

  if (EEPROM.read(EEPROM_ADDR_VERSION) != CONFIG_VERSION) {
    return false;
  }

  uint16_t storedChecksum = (EEPROM.read(EEPROM_ADDR_CHECKSUM) << 8) |
                            EEPROM.read(EEPROM_ADDR_CHECKSUM + 1);
  if (storedChecksum != calculateChecksum()) {
    return false;
  }

  config.potFlags = EEPROM.read(EEPROM_ADDR_FLAGS) & POT_FLAGS_IGNORE_MASK;
  config.btnChannel = clampMidiChannel(EEPROM.read(EEPROM_ADDR_BTN_CH),
                                       DEFAULT_BTN_MIDI_CHANNEL);
  config.potChannel = clampMidiChannel(EEPROM.read(EEPROM_ADDR_POT_CH),
                                       DEFAULT_POT_MIDI_CHANNEL);

  for (int i = 0; i < MAX_BUTTONS; i++) {
    config.buttonNotes[i] = EEPROM.read(EEPROM_ADDR_BUTTONS + i);
  }
  for (int i = 0; i < MAX_POTS; i++) {
    config.potTypes[i] = EEPROM.read(EEPROM_ADDR_POTS + (i * 2));
    config.potValues[i] = EEPROM.read(EEPROM_ADDR_POTS + (i * 2) + 1);
  }
  return true;
}

// ============================================================================
// SYSEX CONFIG PROTOCOL (USB MIDI only)
// ============================================================================

void sendSysEx(byte cmd, const byte *payload, unsigned len) {
  // F0 MFG DEV0 DEV1 CMD [payload...] F7
  byte buf[8 + SX_CONFIG_LEN];
  if (len > SX_CONFIG_LEN) len = SX_CONFIG_LEN;

  unsigned n = 0;
  buf[n++] = 0xF0;
  buf[n++] = SYSEX_MFG;
  buf[n++] = SYSEX_DEV0;
  buf[n++] = SYSEX_DEV1;
  buf[n++] = cmd;
  for (unsigned i = 0; i < len; i++) {
    buf[n++] = payload[i] & 0x7F;
  }
  buf[n++] = 0xF7;
  USB_MIDI.sendSysEx(n, buf, true);
}

void sendAck(byte cmd, byte status) {
  byte payload[2] = {cmd, status};
  sendSysEx(SX_ACK, payload, 2);
}

void sendVersion() {
  const char *ver = PICO_VERSION;
  byte payload[16];
  unsigned len = 0;
  while (ver[len] && len < sizeof(payload)) {
    payload[len] = (byte)ver[len];
    len++;
  }
  sendSysEx(SX_VERSION, payload, len);
}

void sendConfigDump() {
  byte payload[SX_CONFIG_LEN];
  payload[0] = config.btnChannel;
  payload[1] = config.potChannel;
  payload[2] = config.potFlags & POT_FLAGS_IGNORE_MASK;
  for (int i = 0; i < MAX_BUTTONS; i++) {
    payload[3 + i] = config.buttonNotes[i] & 0x7F;
  }
  for (int i = 0; i < MAX_POTS; i++) {
    payload[23 + i] = config.potTypes[i] & 0x7F;
    payload[27 + i] = config.potValues[i] & 0x7F;
  }
  sendSysEx(SX_CONFIG, payload, SX_CONFIG_LEN);
}

void handleSysEx(byte *array, unsigned size) {
  // Expect: F0 7D 54 54 CMD [..] F7  → size >= 6
  if (size < 6) return;
  if (array[0] != 0xF0) return;
  if (array[size - 1] != 0xF7) return;
  if (array[1] != SYSEX_MFG || array[2] != SYSEX_DEV0 || array[3] != SYSEX_DEV1) {
    return;
  }

  byte cmd = array[4];
  byte *data = &array[5];
  unsigned dataLen = size - 6; // exclude F0..CMD and F7

  switch (cmd) {
    case SX_GET_VERSION:
      sendVersion();
      break;

    case SX_GET_CONFIG:
      sendConfigDump();
      break;

    case SX_SET_BTN_NOTE:
      if (dataLen >= 2 && data[0] >= 1 && data[0] <= MAX_BUTTONS && data[1] <= 127) {
        config.buttonNotes[data[0] - 1] = data[1];
        sendAck(cmd, SX_STATUS_OK);
      } else {
        sendAck(cmd, SX_STATUS_ERR);
      }
      break;

    case SX_SET_POT:
      if (dataLen >= 3 && data[0] >= 1 && data[0] <= MAX_POTS) {
        byte pot = data[0] - 1;
        if (data[1] == POT_TYPE_PITCHBEND) {
          config.potTypes[pot] = POT_TYPE_PITCHBEND;
          sendAck(cmd, SX_STATUS_OK);
        } else if (data[1] == POT_TYPE_CC && data[2] <= 127) {
          config.potTypes[pot] = POT_TYPE_CC;
          config.potValues[pot] = data[2];
          sendAck(cmd, SX_STATUS_OK);
        } else {
          sendAck(cmd, SX_STATUS_ERR);
        }
      } else {
        sendAck(cmd, SX_STATUS_ERR);
      }
      break;

    case SX_SET_POT_IGNORE:
      if (dataLen >= 2 && data[0] >= 1 && data[0] <= MAX_POTS && data[1] <= 1) {
        setPotIgnored(data[0] - 1, data[1] == 1);
        sendAck(cmd, SX_STATUS_OK);
      } else {
        sendAck(cmd, SX_STATUS_ERR);
      }
      break;

    case SX_SET_BTN_CH:
      if (dataLen >= 1 && data[0] >= 1 && data[0] <= 16) {
        config.btnChannel = data[0];
        sendAck(cmd, SX_STATUS_OK);
      } else {
        sendAck(cmd, SX_STATUS_ERR);
      }
      break;

    case SX_SET_POT_CH:
      if (dataLen >= 1 && data[0] >= 1 && data[0] <= 16) {
        config.potChannel = data[0];
        sendAck(cmd, SX_STATUS_OK);
      } else {
        sendAck(cmd, SX_STATUS_ERR);
      }
      break;

    case SX_SAVE:
      saveConfig();
      sendAck(cmd, SX_STATUS_OK);
      break;

    case SX_LOAD:
      if (loadConfig()) {
        sendAck(cmd, SX_STATUS_OK);
        sendConfigDump();
      } else {
        sendAck(cmd, SX_STATUS_ERR);
      }
      break;

    case SX_RESET:
      resetConfigToDefaults();
      sendAck(cmd, SX_STATUS_OK);
      sendConfigDump();
      break;

    default:
      sendAck(cmd, SX_STATUS_ERR);
      break;
  }
}

void flashLED(uint32_t color) {
  led.setPixelColor(0, color);
  led.show();
  ledOnTime = millis();
  ledActive = true;
}

void sendMomentaryNote(byte note, uint32_t color) {
  flashLED(color);
  SERIAL_MIDI.sendNoteOn(note, 127, config.btnChannel);
  USB_MIDI.sendNoteOn(note, 127, config.btnChannel);
  delay(5);
  SERIAL_MIDI.sendNoteOff(note, 0, config.btnChannel);
  USB_MIDI.sendNoteOff(note, 0, config.btnChannel);
}

void updateLED() {
  if (ledActive && (millis() - ledOnTime) >= LED_FLASH_MS) {
    led.clear();
    led.show();
    ledActive = false;
  }
}

void Scan_User() {
  unsigned long currentTime = millis();
  int bank = readBank();
  int bankOffset = bank * NUM_NOTE_BUTTONS;

  auto checkButton = [&](int pin, bool &lastRawReading, bool &lastStableState,
                         unsigned long &lastDebounceTime, int buttonIndex) {
    bool reading = digitalRead(pin);
    if (reading != lastRawReading) {
      lastDebounceTime = currentTime;
    }
    lastRawReading = reading;

    if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (reading == LOW && lastStableState == HIGH) {
        int noteIndex = bankOffset + buttonIndex;
        sendMomentaryNote(config.buttonNotes[noteIndex], BANK_COLORS[bank]);
      }
      lastStableState = reading;
    }
  };

  checkButton(BUTTON_PIN1, lastRawReading1, lastStableState1, lastDebounceTime1, 0);
  checkButton(BUTTON_PIN2, lastRawReading2, lastStableState2, lastDebounceTime2, 1);
  checkButton(BUTTON_PIN3, lastRawReading3, lastStableState3, lastDebounceTime3, 2);
  checkButton(BUTTON_PIN4, lastRawReading4, lastStableState4, lastDebounceTime4, 3);
  checkButton(BUTTON_PIN5, lastRawReading5, lastStableState5, lastDebounceTime5, 4);
}

void Scan_Pots() {
  for (int i = 0; i < MAX_POTS; i++) {
    if (isPotIgnored(i)) {
      lastMidiValue[i] = -1;
      continue;
    }

    analogRead(POT_PINS[i]);
    delayMicroseconds(50);
    int raw = analogRead(POT_PINS[i]);
    potSmoothed[i] = (int)(POT_ALPHA * raw + (1.0 - POT_ALPHA) * potSmoothed[i]);
    int smoothed = potSmoothed[i];

    if (config.potTypes[i] == POT_TYPE_PITCHBEND) {
      int pb = map(smoothed, 0, 1023, -8192, 8191);
      int pb7 = smoothed >> 3;
      if (lastMidiValue[i] < 0 || abs(pb7 - lastMidiValue[i]) >= 1) {
        SERIAL_MIDI.sendPitchBend(pb, config.potChannel);
        USB_MIDI.sendPitchBend(pb, config.potChannel);
        lastMidiValue[i] = pb7;
      }
    } else {
      int cc = smoothed >> 3;
      if (cc > 127) cc = 127;
      if (lastMidiValue[i] < 0 || cc != lastMidiValue[i]) {
        byte ccNum = config.potValues[i];
        SERIAL_MIDI.sendControlChange(ccNum, cc, config.potChannel);
        USB_MIDI.sendControlChange(ccNum, cc, config.potChannel);
        lastMidiValue[i] = cc;
      }
    }
  }
}

void loop() {
  Scan_User();
  Scan_Pots();
  updateLED();
  USB_MIDI.read();
  SERIAL_MIDI.read();
}
