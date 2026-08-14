/*
 * OS MIDI Controller
 * Outputs on USB MIDI and Serial MIDI (TX0)
 * Configuration stored in RP2040 emulated EEPROM (flash)
 */

#define PICO_VERSION "v1.5"

#include "config_eeprom.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_VL53L0X.h>
#include <EEPROM.h>
#include <MIDI.h>
#include <Wire.h>

// Forward declarations
uint16_t calculateChecksum();
void resetConfigToDefaults();
void saveConfig();
bool loadConfig();
void initInputPins();
int readBank();
const char *hardwareModeName(byte mode);
bool isKeypadHardwareMode(byte mode);
void sendMomentaryNote(byte note, uint32_t color);
void resetKeypadAccessBuffer();
void handleKeypadAccessKey(int keyIdx);

// ── Button mode (HW_MODE_BUTTONS): pins 2-8 ──────────────────────────────────
// 5 note buttons (pins 2-6)
#define BUTTON_PIN1 2
#define BUTTON_PIN2 3
#define BUTTON_PIN3 4
#define BUTTON_PIN4 5
#define BUTTON_PIN5 6

// 2 bank selector pins (binary: 4 banks)
// GPIO8 = bit 0, GPIO7 = bit 1; HIGH is the active value
#define BANK_SEL_PIN0 8  // Bit 0
#define BANK_SEL_PIN1 7  // Bit 1

// ── Keypad mode (HW_MODE_KEYPAD): pins 2-9 ───────────────────────────────────
// Physical keypad rows connect to pins 6-9 (driven OUTPUT LOW during scan).
// Physical keypad cols connect to pins 2-5 (read as INPUT_PULLUP).
#define KEYPAD_ROW0 6
#define KEYPAD_ROW1 7
#define KEYPAD_ROW2 8
#define KEYPAD_ROW3 9
#define KEYPAD_COL0 2
#define KEYPAD_COL1 3
#define KEYPAD_COL2 4
#define KEYPAD_COL3 5
const int KEYPAD_ROW_PINS[4] = {KEYPAD_ROW0, KEYPAD_ROW1, KEYPAD_ROW2, KEYPAD_ROW3};
const int KEYPAD_COL_PINS[4] = {KEYPAD_COL0, KEYPAD_COL1, KEYPAD_COL2, KEYPAD_COL3};
#define KEYPAD_LED_COLOR 0x00FFFF  // Cyan flash in keypad mode

// Built-in WS2812 RGB LED on RP2040 Zero
#define LED_PIN 16
#define LED_COUNT 1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Non-blocking LED flash state
unsigned long ledOnTime = 0;
bool ledActive = false;
const unsigned long LED_FLASH_MS = 100; // Flash duration

// Bank colors: Red, Green, Blue, White
const uint32_t BANK_COLORS[NUM_BANKS] = {
    0xFF0000, // Bank 0: Red
    0x00FF00, // Bank 1: Green
    0x0000FF, // Bank 2: Blue
    0xFFFFFF  // Bank 3: White
};

#define POTENTIOMETER_PIN1 A0
#define POTENTIOMETER_PIN2 A1
#define POTENTIOMETER_PIN3 A2
#define POTENTIOMETER_PIN4 A3

// Emulated EEPROM size (stored in RP2040 flash)
// Note: Flash has limited write cycles (~100K), don't write too frequently
#define EEPROM_SIZE 512 // 512 bytes (max 4096)

// USB CDC (Serial) for debugging - must be declared to exist
Adafruit_USBD_CDC USBSerial;

// USB MIDI setup
Adafruit_USBD_MIDI usbd_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbd_midi, USB_MIDI);

// Hardware Serial MIDI setup (TX0 = GPIO0)
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, SERIAL_MIDI);

// Debounce variables for 5 note buttons
// Track previous raw reading, last stable state, and last state change time
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
const unsigned long DEBOUNCE_DELAY = 50; // 50ms debounce delay

// Keypad debounce state (16 keys, indexed 0-15)
bool keyRawState[MAX_KEYPAD_KEYS];
bool keyStableState[MAX_KEYPAD_KEYS];
unsigned long keyDebounceTime[MAX_KEYPAD_KEYS];
char keypadAccessBuffer[4] = "";
byte keypadAccessLen = 0;
bool keypadAccessOverflow = false;

// Potentiometer state
const int POT_PINS[MAX_POTS] = {A0, A1, A2, A3};
int potSmoothed[MAX_POTS] = {0, 0, 0, 0};   // Smoothed ADC value (0-1023)
int lastMidiValue[MAX_POTS] = {-1, -1, -1, -1}; // Last sent MIDI value (-1 = never sent)
const int POT_THRESHOLD = 4;  // ADC noise threshold (prevents jitter, ~0.4%)
const float POT_ALPHA = 0.15; // Smoothing factor (0.0=slow, 1.0=no smoothing)
bool adcDebugMode = false;    // Toggle with /a command
unsigned long lastAdcPrint = 0;
const unsigned long ADC_PRINT_INTERVAL = 200; // Print every 200ms

// Sonic2 ultrasonic distance sensor (I2C, direct Wire access)
bool sonicDetected = false;
bool sonicTriggered = false;
unsigned long sonicTriggerTime = 0;
float sonicSmoothed = 0;
int lastSonicMidi = -1;
unsigned long lastSonicRead = 0;
const unsigned long SONIC_POLL_MS = 150;
const unsigned long SONIC_MEASURE_MS = 130;
const float SONIC_MIN_MM = 20.0;
const float SONIC_MAX_MM = 600.0;
const float SONIC_ALPHA = 0.15;
const int SONIC_THRESHOLD = 1;

// VL53L0X ToF distance sensor (I2C, Adafruit library, Wire1)
Adafruit_VL53L0X tof_sensor;
bool tofDetected = false;
float tofSmoothed = 0;
int lastTofMidi = -1;
const float TOF_MIN_MM = 30.0;
const float TOF_MAX_MM = 700.0;
const float TOF_ALPHA = 0.2;
const int TOF_THRESHOLD = 1;

// Configuration in RAM (current working config)
struct Config {
  byte midiChannel;
  byte hwMode;                        // HW_MODE_BUTTONS, HW_MODE_KEYPAD, or HW_MODE_KEYPAD_ACCESS
  byte buttonNotes[MAX_BUTTONS];
  byte keypadNotes[MAX_KEYPAD_KEYS];  // 16 keys in matrix order
  byte potTypes[MAX_POTS];
  byte potValues[MAX_POTS];
  byte potFlags;                      // bits 0-3: ignore pots 1-4
  byte encCCs[MAX_ENCODERS];
  byte encModes[MAX_ENCODERS];
  byte sonicType;
  byte sonicValue;
  byte sonicFlags;
  byte tofType;
  byte tofValue;
  byte tofFlags;
} config;

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

// Read bank from selector pins (active HIGH)
// GPIO8 = bit 0, GPIO7 = bit 1
// Both LOW  = Bank 0
// GPIO8 HIGH = Bank 1
// GPIO7 HIGH = Bank 2
// Both HIGH = Bank 3
int readBank() {
  int bit0 = (digitalRead(BANK_SEL_PIN0) == HIGH) ? 1 : 0;
  int bit1 = (digitalRead(BANK_SEL_PIN1) == HIGH) ? 1 : 0;
  return bit0 | (bit1 << 1);
}

const char *hardwareModeName(byte mode) {
  switch (mode) {
    case HW_MODE_KEYPAD:
      return "KEYPAD";
    case HW_MODE_KEYPAD_ACCESS:
      return "KEYPAD ACCESS";
    default:
      return "BUTTONS";
  }
}

bool isKeypadHardwareMode(byte mode) {
  return mode == HW_MODE_KEYPAD || mode == HW_MODE_KEYPAD_ACCESS;
}

void resetKeypadAccessBuffer() {
  keypadAccessLen = 0;
  keypadAccessOverflow = false;
  keypadAccessBuffer[0] = '\0';
}

void setup() {
  // Initialize all 8 shared input pins as INPUT_PULLUP (works for both modes)
  // Mode-specific debounce state is set by initInputPins() after loadConfig()
  for (int p = 2; p <= 9; p++) {
    pinMode(p, INPUT_PULLUP);
  }

  // Initialize built-in RGB LED
  led.begin();
  led.setBrightness(30);
  led.clear();
  led.show();

  // Initialize potentiometer smoothing with current readings
  // Double-read each to avoid RP2040 ADC crosstalk
  for (int i = 0; i < MAX_POTS; i++) {
    analogRead(POT_PINS[i]);       // Discard (mux settling)
    delayMicroseconds(50);
    potSmoothed[i] = analogRead(POT_PINS[i]); // Actual
  }

  // Initialize TinyUSB Device FIRST
  TinyUSB_Device_Init(0);

  // Set USB descriptors
  USBDevice.setManufacturerDescriptor("OS MIDI Controller");
  USBDevice.setProductDescriptor("MIDI Controller");

  // Initialize USB CDC (Serial debug)
  USBSerial.begin(115200);
  Serial.begin(115200);

  // Start USB MIDI
  USB_MIDI.begin(MIDI_CHANNEL_OMNI);

  delay(100); // Give USB time to enumerate

  // Start Hardware Serial MIDI on TX0 (GPIO0) at standard MIDI baud rate
  Serial1.setTX(0); // TX0 = GPIO0
  SERIAL_MIDI.begin(MIDI_CHANNEL_OMNI);

  // Initialize I2C1 on GPIO10 (SDA1) / GPIO11 (SCL1)
  Wire1.setSDA(10);
  Wire1.setSCL(11);
  Wire1.begin();
  Wire1.setClock(100000L);
  Wire1.setTimeout(200);

  // Probe for Sonic2 sensor at 0x57 (give it time to power up)
  delay(200);
  Wire1.beginTransmission(SONIC_I2C_ADDR);
  if (Wire1.endTransmission() == 0) {
    sonicDetected = true;
  }

  // Probe and initialize VL53L0X ToF sensor at 0x29
  Wire1.beginTransmission(TOF_I2C_ADDR);
  if (Wire1.endTransmission() == 0) {
    if (tof_sensor.begin(TOF_I2C_ADDR, false, &Wire1)) {
      tofDetected = true;
      tof_sensor.startRangeContinuous(50); // 50 ms timing budget
    }
  }

  // Initialize emulated EEPROM (stored in flash)
  EEPROM.begin(EEPROM_SIZE);

  // Wait for serial port to connect (with timeout)
  unsigned long timeout = millis() + 3000; // 3 second timeout
  while (!Serial && millis() < timeout) {
    delay(10);
  }

  delay(100);

  Serial.println("\n=== OS MIDI Controller ===");
  Serial.println("Version: " PICO_VERSION);
  Serial.println("USB MIDI: Active");
  Serial.println("Serial MIDI (TX0): Active");
  Serial.println("EEPROM: Emulated (flash)");
  Serial.print("Sonic2  (0x57): ");
  Serial.println(sonicDetected ? "Detected" : "Not found");
  Serial.print("VL53L0X (0x29): ");
  Serial.println(tofDetected ? "Detected" : "Not found");
  Serial.println("Type /h for help\n");

  // Auto-load configuration from EEPROM
  if (!loadConfig()) {
    Serial.println("No valid config in EEPROM, using factory defaults");
    resetConfigToDefaults();
  }

  // Initialize pins and debounce state based on loaded mode
  initInputPins();

  Serial.print("Hardware mode: ");
  if (config.hwMode == HW_MODE_KEYPAD_ACCESS) {
    Serial.println("KEYPAD ACCESS (type note digits, # sends)");
  } else if (config.hwMode == HW_MODE_KEYPAD) {
    Serial.println("KEYPAD (4x4 matrix, pins 2-9)");
  } else {
    Serial.println("BUTTONS (5 buttons + 4 banks, pins 2-8)");
  }
  Serial.println("Ready!\n");
}

// ============================================================================
// PIN & DEBOUNCE INITIALIZATION
// ============================================================================

// Call after loadConfig() / mode change to set up debounce state for the
// active hardware mode. All 8 shared pins are already INPUT_PULLUP.
void initInputPins() {
  unsigned long now = millis();

  if (isKeypadHardwareMode(config.hwMode)) {
    // Keypad modes: all 8 pins stay INPUT_PULLUP; rows are driven LOW only
    // during scanning, then immediately restored to INPUT_PULLUP.
    for (int i = 0; i < MAX_KEYPAD_KEYS; i++) {
      keyRawState[i]    = HIGH;
      keyStableState[i] = HIGH;
      keyDebounceTime[i] = now;
    }
    resetKeypadAccessBuffer();
  } else {
    // Button mode: read actual pin states so no phantom presses on start
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
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

uint16_t calculateChecksum() {
  uint16_t sum = 0;
  for (uint16_t addr = 0; addr < EEPROM_ADDR_CHECKSUM; addr++) {
    sum += EEPROM.read(addr);
  }
  return sum;
}

void resetConfigToDefaults() {
  Serial.println("Resetting to factory defaults...");

  config.midiChannel = DEFAULT_MIDI_CHANNEL;
  config.hwMode      = DEFAULT_HW_MODE;

  for (int i = 0; i < MAX_BUTTONS; i++) {
    config.buttonNotes[i] = DEFAULT_BUTTON_NOTES[i];
  }
  for (int i = 0; i < MAX_KEYPAD_KEYS; i++) {
    config.keypadNotes[i] = DEFAULT_KEYPAD_NOTES[i];
  }
  for (int i = 0; i < MAX_POTS; i++) {
    config.potTypes[i]  = DEFAULT_POT_TYPES[i];
    config.potValues[i] = DEFAULT_POT_VALUES[i];
  }
  config.potFlags = 0;
  for (int i = 0; i < MAX_ENCODERS; i++) {
    config.encCCs[i]   = DEFAULT_ENC_CCS[i];
    config.encModes[i] = DEFAULT_ENC_MODES[i];
  }

  config.sonicType  = DEFAULT_SONIC_TYPE;
  config.sonicValue = DEFAULT_SONIC_VALUE;
  config.sonicFlags = DEFAULT_SONIC_FLAGS;

  config.tofType  = DEFAULT_TOF_TYPE;
  config.tofValue = DEFAULT_TOF_VALUE;
  config.tofFlags = DEFAULT_TOF_FLAGS;

  Serial.println("Factory defaults loaded to RAM");
}

void saveConfig() {
  Serial.println("\n=== Saving Configuration to EEPROM ===");

  // Write header
  EEPROM.write(EEPROM_ADDR_MAGIC + 0, CONFIG_MAGIC_0);
  EEPROM.write(EEPROM_ADDR_MAGIC + 1, CONFIG_MAGIC_1);
  EEPROM.write(EEPROM_ADDR_MAGIC + 2, CONFIG_MAGIC_2);
  EEPROM.write(EEPROM_ADDR_MAGIC + 3, CONFIG_MAGIC_3);
  EEPROM.write(EEPROM_ADDR_VERSION, CONFIG_VERSION);
  EEPROM.write(EEPROM_ADDR_FLAGS, config.potFlags & POT_FLAGS_IGNORE_MASK);
  EEPROM.write(EEPROM_ADDR_NUM_BTNS, MAX_BUTTONS);
  EEPROM.write(EEPROM_ADDR_NUM_POTS, MAX_POTS);
  EEPROM.write(EEPROM_ADDR_NUM_ENCS, MAX_ENCODERS);
  EEPROM.write(EEPROM_ADDR_CHANNEL, config.midiChannel);
  EEPROM.write(EEPROM_ADDR_MODE, config.hwMode);

  // Write button mappings
  for (int i = 0; i < MAX_BUTTONS; i++) {
    EEPROM.write(EEPROM_ADDR_BUTTONS + i, config.buttonNotes[i]);
  }

  // Write pot mappings
  for (int i = 0; i < MAX_POTS; i++) {
    EEPROM.write(EEPROM_ADDR_POTS + (i * 2), config.potTypes[i]);
    EEPROM.write(EEPROM_ADDR_POTS + (i * 2) + 1, config.potValues[i]);
  }

  // Write encoder mappings
  for (int i = 0; i < MAX_ENCODERS; i++) {
    EEPROM.write(EEPROM_ADDR_ENCODERS + (i * 2), config.encCCs[i]);
    EEPROM.write(EEPROM_ADDR_ENCODERS + (i * 2) + 1, config.encModes[i]);
  }

  // Write Sonic2 config
  EEPROM.write(EEPROM_ADDR_SONIC + 0, config.sonicType);
  EEPROM.write(EEPROM_ADDR_SONIC + 1, config.sonicValue);
  EEPROM.write(EEPROM_ADDR_SONIC + 2, config.sonicFlags);

  // Write ToF VL53L0X config
  EEPROM.write(EEPROM_ADDR_TOF + 0, config.tofType);
  EEPROM.write(EEPROM_ADDR_TOF + 1, config.tofValue);
  EEPROM.write(EEPROM_ADDR_TOF + 2, config.tofFlags);

  // Write keypad note mappings
  for (int i = 0; i < MAX_KEYPAD_KEYS; i++) {
    EEPROM.write(EEPROM_ADDR_KEYPAD + i, config.keypadNotes[i]);
  }

  // Calculate and write checksum
  uint16_t checksum = calculateChecksum();
  EEPROM.write(EEPROM_ADDR_CHECKSUM, (byte)(checksum >> 8));
  EEPROM.write(EEPROM_ADDR_CHECKSUM + 1, (byte)(checksum & 0xFF));

  // Commit changes to flash (required for emulated EEPROM)
  if (EEPROM.commit()) {
    Serial.println("Configuration saved successfully!");
  } else {
    Serial.println("ERROR: EEPROM commit failed!");
  }
  Serial.println("====================\n");
}

bool loadConfig() {
  Serial.println("\n=== Loading Configuration from EEPROM ===");

  // Check magic bytes
  if (EEPROM.read(EEPROM_ADDR_MAGIC + 0) != CONFIG_MAGIC_0 ||
      EEPROM.read(EEPROM_ADDR_MAGIC + 1) != CONFIG_MAGIC_1 ||
      EEPROM.read(EEPROM_ADDR_MAGIC + 2) != CONFIG_MAGIC_2 ||
      EEPROM.read(EEPROM_ADDR_MAGIC + 3) != CONFIG_MAGIC_3) {
    Serial.println("EEPROM not initialized (magic bytes missing)");
    return false;
  }

  // Check version
  byte version = EEPROM.read(EEPROM_ADDR_VERSION);
  if (version != CONFIG_VERSION) {
    Serial.print("Version mismatch: ");
    Serial.println(version);
    return false;
  }

  // Verify checksum
  uint16_t storedChecksum = (EEPROM.read(EEPROM_ADDR_CHECKSUM) << 8) |
                            EEPROM.read(EEPROM_ADDR_CHECKSUM + 1);
  uint16_t calcChecksum = calculateChecksum();

  if (storedChecksum != calcChecksum) {
    Serial.println("Checksum mismatch! EEPROM may be corrupted");
    return false;
  }

  // Load potentiometer ignore flags
  config.potFlags = EEPROM.read(EEPROM_ADDR_FLAGS) & POT_FLAGS_IGNORE_MASK;

  // Load global MIDI channel
  config.midiChannel = EEPROM.read(EEPROM_ADDR_CHANNEL);
  if (config.midiChannel < 1 || config.midiChannel > 16) {
    config.midiChannel = DEFAULT_MIDI_CHANNEL;
  }

  // Load hardware mode
  config.hwMode = EEPROM.read(EEPROM_ADDR_MODE);
  if (config.hwMode != HW_MODE_BUTTONS &&
      config.hwMode != HW_MODE_KEYPAD &&
      config.hwMode != HW_MODE_KEYPAD_ACCESS) {
    config.hwMode = DEFAULT_HW_MODE;
  }

  // Load button mappings
  for (int i = 0; i < MAX_BUTTONS; i++) {
    config.buttonNotes[i] = EEPROM.read(EEPROM_ADDR_BUTTONS + i);
  }

  // Load pot mappings
  for (int i = 0; i < MAX_POTS; i++) {
    config.potTypes[i] = EEPROM.read(EEPROM_ADDR_POTS + (i * 2));
    config.potValues[i] = EEPROM.read(EEPROM_ADDR_POTS + (i * 2) + 1);
  }

  // Load encoder mappings
  for (int i = 0; i < MAX_ENCODERS; i++) {
    config.encCCs[i] = EEPROM.read(EEPROM_ADDR_ENCODERS + (i * 2));
    config.encModes[i] = EEPROM.read(EEPROM_ADDR_ENCODERS + (i * 2) + 1);
  }

  // Load Sonic2 config
  config.sonicType  = EEPROM.read(EEPROM_ADDR_SONIC + 0);
  config.sonicValue = EEPROM.read(EEPROM_ADDR_SONIC + 1);
  config.sonicFlags = EEPROM.read(EEPROM_ADDR_SONIC + 2);

  // Load ToF VL53L0X config
  config.tofType  = EEPROM.read(EEPROM_ADDR_TOF + 0);
  config.tofValue = EEPROM.read(EEPROM_ADDR_TOF + 1);
  config.tofFlags = EEPROM.read(EEPROM_ADDR_TOF + 2);

  // Load keypad note mappings
  for (int i = 0; i < MAX_KEYPAD_KEYS; i++) {
    config.keypadNotes[i] = EEPROM.read(EEPROM_ADDR_KEYPAD + i);
  }

  Serial.println("Configuration loaded successfully!");
  Serial.println("====================\n");
  return true;
}

void printHelp() {
  Serial.println("\n=== Command Help ===");
  Serial.println("MODE:");
  Serial.println("  /mode 0             - Button mode (5 buttons + 4 banks, pins 2-8)");
  Serial.println("  /mode 1             - Keypad mode (4x4 matrix, pins 2-9)");
  Serial.println("  /mode 2             - Keypad access mode (type note digits, # sends)");
  Serial.println("");
  Serial.println("BUTTON MAPPING (mode 0):");
  Serial.println("  /b <1-20> <0-127>   - Map button slot to MIDI note");
  Serial.println("     Slots: Bank0=1-5, Bank1=6-10, Bank2=11-15, Bank3=16-20");
  Serial.println("");
  Serial.println("KEYPAD MAPPING (mode 1):");
  Serial.println("  /k <1-16> <0-127>   - Map keypad key slot to MIDI note");
  Serial.println("     Layout: 1=1  2=2  3=3  4=A");
  Serial.println("             5=4  6=5  7=6  8=B");
  Serial.println("             9=7 10=8 11=9 12=C");
  Serial.println("            13=* 14=0 15=# 16=D");
  Serial.println("");
  Serial.println("KEYPAD ACCESS (mode 2):");
  Serial.println("  Digits 0-9 build a MIDI note number; # sends it; * clears it");
  Serial.println("  Example: press 2, 8, # to send MIDI note 28");
  Serial.println("");
  Serial.println("SHARED MAPPING:");
  Serial.println("  /p <1-4> cc <0-127> - Map pot to CC number");
  Serial.println("  /p <1-4> pb         - Map pot to Pitch Bend");
  Serial.println("  /pi <1-4> <0|1>     - Ignore pot ADC input (1=ignore, 0=active)");
  Serial.println("  /ch <1-16>          - Set global MIDI channel");
  Serial.println("  /s cc <0-127>       - Map Sonic2 to CC number");
  Serial.println("  /s pb               - Map Sonic2 to Pitch Bend");
  Serial.println("  /t cc <0-127>       - Map VL53L0X ToF to CC number");
  Serial.println("  /t pb               - Map VL53L0X ToF to Pitch Bend");
  Serial.println("  /m                  - Show all current mappings");
  Serial.println("");
  Serial.println("STORAGE:");
  Serial.println("  /save   - Save configuration to EEPROM");
  Serial.println("  /load   - Load configuration from EEPROM");
  Serial.println("  /reset  - Reset to factory defaults");
  Serial.println("");
  Serial.println("INFO:");
  Serial.println("  /h    - Show this help");
  Serial.println("  /v    - Show firmware version");
  Serial.println("  /d    - Dump EEPROM contents");
  Serial.println("  /test - Test input states");
  Serial.println("  /a    - Toggle ADC debug (raw pot values)");
  Serial.println("  /i    - Scan I2C bus (Wire1: GP10/GP11)");
  Serial.println("====================\n");
}

void printMidiMapping() {
  Serial.println("\n=== Current MIDI Mapping ===");

  Serial.print("MIDI Channel: ");
  Serial.println(config.midiChannel);

  Serial.print("Hardware Mode: ");
  Serial.print(hardwareModeName(config.hwMode));
  Serial.print(" (");
  Serial.print(config.hwMode);
  Serial.println(")");

  if (config.hwMode == HW_MODE_BUTTONS) {
    int currentBank = readBank();
    Serial.print("Active Bank: ");
    Serial.print(currentBank);
    Serial.print(" (Pin8=");
    Serial.print(digitalRead(BANK_SEL_PIN0) == HIGH ? "HIGH" : "LOW");
    Serial.print(", Pin7=");
    Serial.print(digitalRead(BANK_SEL_PIN1) == HIGH ? "HIGH" : "LOW");
    Serial.println(")");

    Serial.println("\nBUTTONS (5 buttons x 4 banks):");
    for (int bank = 0; bank < NUM_BANKS; bank++) {
      Serial.print("  Bank ");
      Serial.print(bank);
      if (bank == currentBank) Serial.print(" *");
      Serial.println(":");
      for (int btn = 0; btn < NUM_NOTE_BUTTONS; btn++) {
        int idx = bank * NUM_NOTE_BUTTONS + btn;
        Serial.print("    Btn ");
        Serial.print(btn + 1);
        Serial.print(" -> Note ");
        Serial.println(config.buttonNotes[idx]);
      }
    }
  } else if (config.hwMode == HW_MODE_KEYPAD) {
    Serial.println("\nKEYPAD (4x4 matrix):");
    for (int i = 0; i < MAX_KEYPAD_KEYS; i++) {
      Serial.print("  Slot ");
      if (i + 1 < 10) Serial.print(" ");
      Serial.print(i + 1);
      Serial.print(" [");
      Serial.print(KEYPAD_LABELS[i]);
      Serial.print("] -> Note ");
      Serial.println(config.keypadNotes[i]);
    }
  } else {
    Serial.println("\nKEYPAD ACCESS:");
    Serial.println("  Digits 0-9 build the MIDI note number");
    Serial.println("  # sends the buffered note; * clears the buffer");
    Serial.print("  Pending buffer: ");
    Serial.println(keypadAccessLen > 0 ? keypadAccessBuffer : "(empty)");
  }

  Serial.println("\nPOTENTIOMETERS:");
  for (int i = 0; i < MAX_POTS; i++) {
    Serial.print("  Pot ");
    Serial.print(i + 1);
    Serial.print(" (A");
    Serial.print(i);
    Serial.print(") -> ");

    if (config.potTypes[i] == POT_TYPE_PITCHBEND) {
      Serial.print("Pitch Bend");
    } else {
      Serial.print("CC ");
      Serial.print(config.potValues[i]);
    }
    if (isPotIgnored(i)) Serial.print(" [ignored]");
    Serial.println();
  }

  Serial.println("\nI2C DEVICES:");
  Serial.print("  Sonic2  (0x57): ");
  if (sonicDetected) {
    if (config.sonicType == SONIC_TYPE_PITCHBEND) {
      Serial.print("Pitch Bend");
    } else {
      Serial.print("CC ");
      Serial.print(config.sonicValue);
    }
    Serial.print(config.sonicFlags & SONIC_FLAG_INVERT ? " [invert]" : "");
    Serial.println(" [active]");
  } else {
    Serial.println("Not connected");
  }

  Serial.print("  VL53L0X (0x29): ");
  if (tofDetected) {
    if (config.tofType == TOF_TYPE_PITCHBEND) {
      Serial.print("Pitch Bend");
    } else {
      Serial.print("CC ");
      Serial.print(config.tofValue);
    }
    Serial.print(config.tofFlags & TOF_FLAG_INVERT ? " [invert]" : "");
    Serial.println(" [active]");
  } else {
    Serial.println("Not connected");
  }

  Serial.println("\nENCODERS: (future)");
  for (int i = 0; i < MAX_ENCODERS; i++) {
    Serial.print("  Encoder ");
    Serial.print(i + 1);
    Serial.print(" -> CC ");
    Serial.print(config.encCCs[i]);
    Serial.print(" (");
    Serial.print(config.encModes[i] == ENC_MODE_RELATIVE ? "Relative"
                                                         : "Absolute");
    Serial.println(")");
  }

  Serial.println("\n====================\n");
}

void dumpEEPROM() {
  Serial.println("\n=== EEPROM Dump (Emulated Flash) ===");
  Serial.print("Size: ");
  Serial.print(EEPROM_SIZE);
  Serial.println(" bytes\n");

  for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += 16) {
    // Print address
    Serial.print("0x");
    if (addr < 0x0100)
      Serial.print("0");
    if (addr < 0x0010)
      Serial.print("0");
    Serial.print(addr, HEX);
    Serial.print(": ");

    // Read and store 16 bytes
    byte data[16];
    for (int i = 0; i < 16; i++) {
      data[i] = EEPROM.read(addr + i);
    }

    // Print hex values
    for (int i = 0; i < 16; i++) {
      if (data[i] < 0x10)
        Serial.print("0");
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }

    Serial.print(" | ");

    // Print ASCII representation
    for (int i = 0; i < 16; i++) {
      if (data[i] >= 32 && data[i] <= 126) {
        Serial.print((char)data[i]);
      } else {
        Serial.print(".");
      }
    }

    Serial.println();

    // Small delay to prevent overwhelming the serial buffer
    delay(5);
  }

  Serial.println("\n=== End of EEPROM Dump ===\n");
}

void scanI2C() {
  Serial.println("\n=== I2C Scan (Wire1: SDA=GP10, SCL=GP11) ===");
  int found = 0;
  bool sonicFound = false;

  bool tofFound = false;

  for (byte addr = 1; addr < 127; addr++) {
    Wire1.beginTransmission(addr);
    byte error = Wire1.endTransmission();

    if (error == 0) {
      Serial.print("  0x");
      if (addr < 0x10) Serial.print("0");
      Serial.print(addr, HEX);
      Serial.print(" (");
      Serial.print(addr);
      Serial.print(") - ");
      if (addr == SONIC_I2C_ADDR) {
        Serial.println("Sonic2 Ultrasonic Sensor");
        sonicFound = true;
      } else if (addr == TOF_I2C_ADDR) {
        Serial.println("VL53L0X ToF Distance Sensor");
        tofFound = true;
      } else {
        Serial.println("Unknown device");
      }
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  No devices found");
  } else {
    Serial.print("  Total: ");
    Serial.print(found);
    Serial.println(" device(s)");
  }

  // Reset I2C bus after scan (scan traffic can confuse devices)
  Wire1.end();
  delay(10);
  Wire1.setSDA(10);
  Wire1.setSCL(11);
  Wire1.begin();
  Wire1.setClock(100000L);
  Wire1.setTimeout(200);
  delay(50);

  // Hot-plug: Sonic2
  if (sonicFound && !sonicDetected) {
    sonicDetected = true;
    sonicTriggered = false;
    Serial.println("  >> Sonic2 activated");
  } else if (!sonicFound && sonicDetected) {
    sonicDetected = false;
    sonicTriggered = false;
    lastSonicMidi = -1;
    Serial.println("  >> Sonic2 deactivated");
  }

  // Hot-plug: VL53L0X (needs re-init after Wire1 reset)
  if (tofFound && !tofDetected) {
    if (tof_sensor.begin(TOF_I2C_ADDR, false, &Wire1)) {
      tofDetected = true;
      tofSmoothed = 0;
      lastTofMidi = -1;
      tof_sensor.startRangeContinuous(50);
      Serial.println("  >> VL53L0X activated");
    } else {
      Serial.println("  >> VL53L0X found but init failed");
    }
  } else if (!tofFound && tofDetected) {
    tofDetected = false;
    lastTofMidi = -1;
    Serial.println("  >> VL53L0X deactivated");
  }

  Serial.println("====================\n");
}

void Process_Serial_Commands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove whitespace

    // Debug: show received command
    if (command.length() > 0) {
      Serial.print("Received: [");
      Serial.print(command);
      Serial.println("]");
    }

    if (command == "/h") {
      printHelp();
    } else if (command == "/v") {
      Serial.println("Firmware Version: " PICO_VERSION);
    } else if (command.startsWith("/mode ")) {
      int mode;
      if (sscanf(command.c_str(), "/mode %d", &mode) == 1) {
        if (mode == HW_MODE_BUTTONS || mode == HW_MODE_KEYPAD ||
            mode == HW_MODE_KEYPAD_ACCESS) {
          config.hwMode = mode;
          initInputPins();
          Serial.print("Hardware mode: ");
          if (mode == HW_MODE_KEYPAD_ACCESS) {
            Serial.println("KEYPAD ACCESS (type note digits, # sends)");
          } else if (mode == HW_MODE_KEYPAD) {
            Serial.println("KEYPAD (4x4 matrix, pins 2-9)");
          } else {
            Serial.println("BUTTONS (5 buttons + 4 banks, pins 2-8)");
          }
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: Mode must be 0 (buttons), 1 (keypad), or 2 (keypad access)");
        }
      } else {
        Serial.println("Usage: /mode 0|1|2");
      }
    } else if (command.startsWith("/k ")) {
      int keyNum, noteNum;
      if (sscanf(command.c_str(), "/k %d %d", &keyNum, &noteNum) == 2) {
        if (keyNum >= 1 && keyNum <= MAX_KEYPAD_KEYS && noteNum >= 0 && noteNum <= 127) {
          config.keypadNotes[keyNum - 1] = noteNum;
          Serial.print("Key [");
          Serial.print(KEYPAD_LABELS[keyNum - 1]);
          Serial.print("] slot ");
          Serial.print(keyNum);
          Serial.print(" -> Note ");
          Serial.println(noteNum);
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: Key slot 1-16, Note 0-127");
        }
      } else {
        Serial.println("Usage: /k <1-16> <0-127>");
      }
    } else if (command == "/m") {
      printMidiMapping();
    } else if (command == "/save") {
      saveConfig();
    } else if (command == "/load") {
      if (loadConfig()) {
        Serial.println("Config loaded. Type /m to view.");
      } else {
        Serial.println("Load failed. Using current RAM config.");
      }
    } else if (command == "/reset") {
      resetConfigToDefaults();
      Serial.println("Type /save to persist, or /m to view.");
    } else if (command.startsWith("/b ")) {
      // Parse: /b <1-16> <0-127>
      int btnNum, noteNum;
      if (sscanf(command.c_str(), "/b %d %d", &btnNum, &noteNum) == 2) {
        if (btnNum >= 1 && btnNum <= MAX_BUTTONS && noteNum >= 0 &&
            noteNum <= 127) {
          config.buttonNotes[btnNum - 1] = noteNum;
          Serial.print("Button ");
          Serial.print(btnNum);
          Serial.print(" mapped to Note ");
          Serial.println(noteNum);
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: Button 1-16, Note 0-127");
        }
      } else {
        Serial.println("Usage: /b <1-16> <0-127>");
      }
    } else if (command.startsWith("/pi ")) {
      // Parse: /pi <1-4> <0|1>
      int potNum, ignored;
      if (sscanf(command.c_str(), "/pi %d %d", &potNum, &ignored) == 2) {
        if (potNum < 1 || potNum > MAX_POTS) {
          Serial.println("Error: Pot must be 1-4");
        } else if (ignored != 0 && ignored != 1) {
          Serial.println("Error: Ignore value must be 0 or 1");
        } else {
          setPotIgnored(potNum - 1, ignored == 1);
          Serial.print("Pot ");
          Serial.print(potNum);
          Serial.println(ignored == 1 ? " ignored" : " active");
          Serial.println("Type /save to persist to EEPROM");
        }
      } else {
        Serial.println("Usage: /pi <1-4> <0|1>");
      }
    } else if (command.startsWith("/p ")) {
      // Parse: /p <1-4> cc <0-127> OR /p <1-4> pb
      int potNum, ccNum;
      char type[10];

      if (sscanf(command.c_str(), "/p %d %s %d", &potNum, type, &ccNum) >= 2) {
        if (potNum < 1 || potNum > MAX_POTS) {
          Serial.println("Error: Pot must be 1-4");
        } else if (strcmp(type, "pb") == 0) {
          // Pitch bend
          config.potTypes[potNum - 1] = POT_TYPE_PITCHBEND;
          Serial.print("Pot ");
          Serial.print(potNum);
          Serial.println(" mapped to Pitch Bend");
          Serial.println("Type /save to persist to EEPROM");
        } else if (strcmp(type, "cc") == 0 && ccNum >= 0 && ccNum <= 127) {
          // CC
          config.potTypes[potNum - 1] = POT_TYPE_CC;
          config.potValues[potNum - 1] = ccNum;
          Serial.print("Pot ");
          Serial.print(potNum);
          Serial.print(" mapped to CC ");
          Serial.println(ccNum);
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: CC must be 0-127");
        }
      } else {
        Serial.println("Usage: /p <1-4> cc <0-127> OR /p <1-4> pb");
      }
    } else if (command == "/d") {
      dumpEEPROM();
    } else if (command == "/test") {
      Serial.print("\n=== Input Test (mode=");
      Serial.print(hardwareModeName(config.hwMode));
      Serial.println(") ===");
      if (config.hwMode == HW_MODE_BUTTONS) {
        int bank = readBank();
        int offset = bank * NUM_NOTE_BUTTONS;
        Serial.print("Bank Sel Pin8 (bit 0): ");
        Serial.println(digitalRead(BANK_SEL_PIN0) == HIGH ? "HIGH (1)" : "LOW (0)");
        Serial.print("Bank Sel Pin7 (bit 1): ");
        Serial.println(digitalRead(BANK_SEL_PIN1) == HIGH ? "HIGH (1)" : "LOW (0)");
        Serial.print("Active Bank: ");
        Serial.println(bank);
        for (int i = 0; i < NUM_NOTE_BUTTONS; i++) {
          int pin = BUTTON_PIN1 + i;
          Serial.print("Btn ");
          Serial.print(i + 1);
          Serial.print(" (pin ");
          Serial.print(pin);
          Serial.print("): ");
          Serial.print(digitalRead(pin) == LOW ? "PRESSED " : "RELEASED");
          Serial.print("  -> Note ");
          Serial.println(config.buttonNotes[offset + i]);
        }
      } else if (config.hwMode == HW_MODE_KEYPAD) {
        Serial.println("Col pins (raw reads, rows idle HIGH):");
        for (int c = 0; c < 4; c++) {
          Serial.print("  Col");
          Serial.print(c);
          Serial.print(" (pin ");
          Serial.print(KEYPAD_COL_PINS[c]);
          Serial.print("): ");
          Serial.println(digitalRead(KEYPAD_COL_PINS[c]) == LOW ? "LOW" : "HIGH");
        }
        Serial.println("Keypad note table:");
        for (int i = 0; i < MAX_KEYPAD_KEYS; i++) {
          Serial.print("  [");
          Serial.print(KEYPAD_LABELS[i]);
          Serial.print("] -> Note ");
          Serial.println(config.keypadNotes[i]);
        }
      } else {
        Serial.println("Keypad access mode:");
        Serial.println("  Press digits 0-9 to enter a MIDI note, # to send, * to clear.");
        Serial.print("  Pending buffer: ");
        Serial.println(keypadAccessLen > 0 ? keypadAccessBuffer : "(empty)");
      }
      Serial.println("==================\n");
    } else if (command.startsWith("/ch ")) {
      int ch;
      if (sscanf(command.c_str(), "/ch %d", &ch) == 1) {
        if (ch >= 1 && ch <= 16) {
          config.midiChannel = ch;
          Serial.print("MIDI channel set to ");
          Serial.println(ch);
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: Channel must be 1-16");
        }
      } else {
        Serial.println("Usage: /ch <1-16>");
      }
    } else if (command.startsWith("/s ")) {
      // Parse: /s cc <0-127> OR /s pb
      int ccNum;
      char type[10];
      if (sscanf(command.c_str(), "/s %s %d", type, &ccNum) >= 1) {
        if (strcmp(type, "pb") == 0) {
          config.sonicType = SONIC_TYPE_PITCHBEND;
          Serial.println("Sonic2 mapped to Pitch Bend");
          Serial.println("Type /save to persist to EEPROM");
        } else if (strcmp(type, "cc") == 0 && ccNum >= 0 && ccNum <= 127) {
          config.sonicType = SONIC_TYPE_CC;
          config.sonicValue = ccNum;
          Serial.print("Sonic2 mapped to CC ");
          Serial.println(ccNum);
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: CC must be 0-127");
        }
      } else {
        Serial.println("Usage: /s cc <0-127> OR /s pb");
      }
    } else if (command.startsWith("/t ")) {
      // Parse: /t cc <0-127> OR /t pb
      int ccNum;
      char type[10];
      if (sscanf(command.c_str(), "/t %s %d", type, &ccNum) >= 1) {
        if (strcmp(type, "pb") == 0) {
          config.tofType = TOF_TYPE_PITCHBEND;
          Serial.println("VL53L0X ToF mapped to Pitch Bend");
          Serial.println("Type /save to persist to EEPROM");
        } else if (strcmp(type, "cc") == 0 && ccNum >= 0 && ccNum <= 127) {
          config.tofType = TOF_TYPE_CC;
          config.tofValue = ccNum;
          Serial.print("VL53L0X ToF mapped to CC ");
          Serial.println(ccNum);
          Serial.println("Type /save to persist to EEPROM");
        } else {
          Serial.println("Error: CC must be 0-127");
        }
      } else {
        Serial.println("Usage: /t cc <0-127> OR /t pb");
      }
    } else if (command == "/a") {
      adcDebugMode = !adcDebugMode;
      Serial.print("ADC debug: ");
      Serial.println(adcDebugMode ? "ON" : "OFF");
    } else if (command == "/st") {
      Serial.println("\n=== Sonic2 I2C Test ===");

      // Test 1: address probe
      Wire1.beginTransmission(SONIC_I2C_ADDR);
      byte e1 = Wire1.endTransmission();
      Serial.print("1. Address probe: err=");
      Serial.println(e1);

      delay(50);

      // Test 2: write 0x01 (trigger)
      Wire1.beginTransmission(SONIC_I2C_ADDR);
      Wire1.write(0x01);
      byte e2 = Wire1.endTransmission();
      Serial.print("2. Write 0x01: err=");
      Serial.println(e2);

      delay(150);

      // Test 3: read 3 bytes
      uint8_t n3 = Wire1.requestFrom((uint8_t)SONIC_I2C_ADDR, (uint8_t)3);
      Serial.print("3. Read 3 bytes: n=");
      Serial.print(n3);
      if (n3 > 0) {
        Serial.print(" data=");
        for (uint8_t i = 0; i < n3; i++) {
          if (i > 0) Serial.print(",");
          Serial.print(Wire1.read(), HEX);
        }
      }
      Serial.println();

      delay(50);

      // Test 4: just read without trigger
      uint8_t n4 = Wire1.requestFrom((uint8_t)SONIC_I2C_ADDR, (uint8_t)3);
      Serial.print("4. Read without trigger: n=");
      Serial.print(n4);
      if (n4 > 0) {
        Serial.print(" data=");
        for (uint8_t i = 0; i < n4; i++) {
          if (i > 0) Serial.print(",");
          Serial.print(Wire1.read(), HEX);
        }
      }
      Serial.println();

      delay(50);

      // Test 5: write 0x00 instead
      Wire1.beginTransmission(SONIC_I2C_ADDR);
      Wire1.write(0x00);
      byte e5 = Wire1.endTransmission();
      Serial.print("5. Write 0x00: err=");
      Serial.println(e5);

      // Test 6: Probe Scroll encoder at 0x40
      Serial.println("\n--- Scroll (0x40) ---");
      Wire1.beginTransmission(0x40);
      byte e6 = Wire1.endTransmission();
      Serial.print("6. Address probe: err=");
      Serial.println(e6);

      if (e6 == 0) {
        delay(10);

        // Try reading raw bytes
        uint8_t n7 = Wire1.requestFrom((uint8_t)0x40, (uint8_t)4);
        Serial.print("7. Read 4 bytes: n=");
        Serial.print(n7);
        if (n7 > 0) {
          Serial.print(" data=");
          for (uint8_t i = 0; i < n7; i++) {
            if (i > 0) Serial.print(",");
            Serial.print(Wire1.read(), HEX);
          }
        }
        Serial.println();

        delay(10);

        // Try writing register 0x10 then reading (common encoder pattern)
        Wire1.beginTransmission(0x40);
        Wire1.write(0x10);
        byte e8 = Wire1.endTransmission();
        Serial.print("8. Write reg 0x10: err=");
        Serial.println(e8);

        if (e8 == 0) {
          delay(10);
          uint8_t n9 = Wire1.requestFrom((uint8_t)0x40, (uint8_t)2);
          Serial.print("9. Read 2 bytes: n=");
          Serial.print(n9);
          if (n9 > 0) {
            Serial.print(" data=");
            for (uint8_t i = 0; i < n9; i++) {
              if (i > 0) Serial.print(",");
              Serial.print(Wire1.read(), HEX);
            }
          }
          Serial.println();
        }
      }

      Serial.println("====================\n");
    } else if (command == "/i") {
      scanI2C();
    } else if (command.length() > 0) {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.println("Type /h for help");
    }
  }
}

// Flash the LED with the given 24-bit RGB color (non-blocking)
void flashLED(uint32_t color) {
  led.setPixelColor(0, color);
  led.show();
  ledOnTime = millis();
  ledActive = true;
}

void sendMomentaryNote(byte note, uint32_t color) {
  flashLED(color);
  SERIAL_MIDI.sendNoteOn(note, 127, config.midiChannel);
  USB_MIDI.sendNoteOn(note, 127, config.midiChannel);
  delay(5);
  SERIAL_MIDI.sendNoteOff(note, 0, config.midiChannel);
  USB_MIDI.sendNoteOff(note, 0, config.midiChannel);
}

// Call in loop() to turn off LED after flash duration
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
  int bankOffset = bank * NUM_NOTE_BUTTONS; // 0, 5, 10, or 15

  // Helper function to check a single button with debouncing
  // buttonIndex is the physical button (0-4), note index = bankOffset + buttonIndex
  auto checkButton = [&](int pin, bool &lastRawReading, bool &lastStableState,
                         unsigned long &lastDebounceTime, int buttonIndex) {
    bool reading = digitalRead(pin);

    // If raw reading changed, reset debounce timer
    if (reading != lastRawReading) {
      lastDebounceTime = currentTime;
    }

    // Update raw reading immediately (for next comparison)
    lastRawReading = reading;

    // If enough time has passed since last state change
    if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
      // Check for falling edge (button press: HIGH -> LOW)
      if (reading == LOW && lastStableState == HIGH) {
        int noteIndex = bankOffset + buttonIndex;
        byte note = config.buttonNotes[noteIndex];
        if (Serial) {
          Serial.print("Btn ");
          Serial.print(buttonIndex + 1);
          Serial.print(" Bank ");
          Serial.print(bank);
          Serial.print(" -> Note ");
          Serial.print(note);
          Serial.print(" [idx ");
          Serial.print(noteIndex);
          Serial.println("]");
        }
        sendMomentaryNote(note, BANK_COLORS[bank]);
      }
      // Update stable state only after debounce period
      lastStableState = reading;
    }
  };

  // Check 5 note buttons with proper debouncing
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

    // RP2040 ADC crosstalk fix: read twice, discard first
    // The ADC mux needs time to settle when switching channels
    analogRead(POT_PINS[i]); // Discard first read (crosstalk)
    delayMicroseconds(50);   // Let mux settle
    int raw = analogRead(POT_PINS[i]); // Actual reading

    // Exponential moving average for smoothing
    potSmoothed[i] = (int)(POT_ALPHA * raw + (1.0 - POT_ALPHA) * potSmoothed[i]);

    // Only send if value changed enough from last sent value
    int smoothed = potSmoothed[i];

    if (config.potTypes[i] == POT_TYPE_PITCHBEND) {
      // Pitch Bend: 14-bit value, map 0-1023 -> -8192 to +8191
      int pb = map(smoothed, 0, 1023, -8192, 8191);
      // Convert to 7-bit equivalent for change detection
      int pb7 = smoothed >> 3; // 0-127 range for threshold check
      if (lastMidiValue[i] < 0 || abs(pb7 - lastMidiValue[i]) >= 1) {
        SERIAL_MIDI.sendPitchBend(pb, config.midiChannel);
        USB_MIDI.sendPitchBend(pb, config.midiChannel);
        lastMidiValue[i] = pb7;
      }
    } else {
      // CC: map 0-1023 -> 0-127
      int cc = smoothed >> 3; // Fast divide by 8 (1024/128 = 8)
      if (cc > 127) cc = 127;

      if (lastMidiValue[i] < 0 || cc != lastMidiValue[i]) {
        byte ccNum = config.potValues[i];
        SERIAL_MIDI.sendControlChange(ccNum, cc, config.midiChannel);
        USB_MIDI.sendControlChange(ccNum, cc, config.midiChannel);
        lastMidiValue[i] = cc;
      }
    }
  }

  // ADC debug: print all 4 raw values on one line, throttled
  if (adcDebugMode && Serial && (millis() - lastAdcPrint >= ADC_PRINT_INTERVAL)) {
    lastAdcPrint = millis();
    for (int i = 0; i < MAX_POTS; i++) {
      analogRead(POT_PINS[i]); // Discard
      delayMicroseconds(50);
      int raw = analogRead(POT_PINS[i]);
      Serial.print("adc");
      Serial.print(i);
      Serial.print(":");
      Serial.print(raw);
      if (i < MAX_POTS - 1) Serial.print("\t");
    }
    Serial.println();
  }
}

void Scan_Sonic() {
  if (!sonicDetected) return;

  // State machine using raw Wire1:
  // IDLE → trigger (write 0x01) → wait 130ms → read 3 bytes → process → IDLE
  if (!sonicTriggered) {
    if (millis() - lastSonicRead < SONIC_POLL_MS) return;

    Wire1.beginTransmission(SONIC_I2C_ADDR);
    Wire1.write(0x01);
    byte err = Wire1.endTransmission();
    if (err != 0) {
      Serial.print("[SONIC] trigger err=");
      Serial.println(err);
      lastSonicRead = millis();
      return;
    }
    sonicTriggered = true;
    sonicTriggerTime = millis();
    return;
  }

  // Waiting for measurement
  if (millis() - sonicTriggerTime < SONIC_MEASURE_MS) return;

  // Read 3 bytes
  sonicTriggered = false;
  lastSonicRead = millis();

  uint8_t n = Wire1.requestFrom((uint8_t)SONIC_I2C_ADDR, (uint8_t)3);
  if (n < 3) {
    Serial.print("[SONIC] read got ");
    Serial.print(n);
    Serial.println(" bytes, expected 3");
    return;
  }
  uint8_t b0 = Wire1.read();
  uint8_t b1 = Wire1.read();
  uint8_t b2 = Wire1.read();
  uint32_t raw = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | b2;
  float dist = (float)raw / 1000.0;

  Serial.print("[SONIC] b=");
  Serial.print(b0, HEX); Serial.print(",");
  Serial.print(b1, HEX); Serial.print(",");
  Serial.print(b2, HEX);
  Serial.print(" raw="); Serial.print(raw);
  Serial.print(" dist="); Serial.println(dist);

  // Clamp to usable range
  if (dist < SONIC_MIN_MM) dist = SONIC_MIN_MM;
  if (dist > SONIC_MAX_MM) dist = SONIC_MAX_MM;

  // EMA smoothing
  if (sonicSmoothed == 0) {
    sonicSmoothed = dist;
  } else {
    sonicSmoothed = SONIC_ALPHA * dist + (1.0 - SONIC_ALPHA) * sonicSmoothed;
  }

  bool invert = (config.sonicFlags & SONIC_FLAG_INVERT);

  if (config.sonicType == SONIC_TYPE_PITCHBEND) {
    int pb;
    if (invert) {
      pb = map((int)sonicSmoothed, (int)SONIC_MIN_MM, (int)SONIC_MAX_MM, 8191, -8192);
    } else {
      pb = map((int)sonicSmoothed, (int)SONIC_MIN_MM, (int)SONIC_MAX_MM, -8192, 8191);
    }
    int pb7 = (int)sonicSmoothed >> 3;
    if (lastSonicMidi < 0 || abs(pb7 - lastSonicMidi) >= SONIC_THRESHOLD) {
      SERIAL_MIDI.sendPitchBend(pb, config.midiChannel);
      USB_MIDI.sendPitchBend(pb, config.midiChannel);
      lastSonicMidi = pb7;
      if (Serial) {
        Serial.print("Sonic2 -> PB ");
        Serial.print(pb);
        Serial.print(" (dist:");
        Serial.print((int)sonicSmoothed);
        Serial.println("mm)");
      }
    }
  } else {
    int cc;
    if (invert) {
      cc = map((int)sonicSmoothed, (int)SONIC_MIN_MM, (int)SONIC_MAX_MM, 127, 0);
    } else {
      cc = map((int)sonicSmoothed, (int)SONIC_MIN_MM, (int)SONIC_MAX_MM, 0, 127);
    }
    if (cc < 0) cc = 0;
    if (cc > 127) cc = 127;

    if (lastSonicMidi < 0 || cc != lastSonicMidi) {
      SERIAL_MIDI.sendControlChange(config.sonicValue, cc, config.midiChannel);
      USB_MIDI.sendControlChange(config.sonicValue, cc, config.midiChannel);
      lastSonicMidi = cc;
      if (Serial) {
        Serial.print("Sonic2 -> CC ");
        Serial.print(config.sonicValue);
        Serial.print(" val:");
        Serial.print(cc);
        Serial.print(" (dist:");
        Serial.print((int)sonicSmoothed);
        Serial.println("mm)");
      }
    }
  }
}

void Scan_ToF() {
  if (!tofDetected) return;
  if (!tof_sensor.isRangeComplete()) return;

  uint16_t raw = tof_sensor.readRangeResult();

  // 8190/8191 = out of range or signal fail
  if (raw >= 8190) return;

  float dist = (float)raw;
  if (dist < TOF_MIN_MM) dist = TOF_MIN_MM;
  if (dist > TOF_MAX_MM) dist = TOF_MAX_MM;

  // EMA smoothing
  if (tofSmoothed == 0) {
    tofSmoothed = dist;
  } else {
    tofSmoothed = TOF_ALPHA * dist + (1.0 - TOF_ALPHA) * tofSmoothed;
  }

  bool invert = (config.tofFlags & TOF_FLAG_INVERT);

  if (config.tofType == TOF_TYPE_PITCHBEND) {
    int pb;
    if (invert) {
      pb = map((int)tofSmoothed, (int)TOF_MIN_MM, (int)TOF_MAX_MM, 8191, -8192);
    } else {
      pb = map((int)tofSmoothed, (int)TOF_MIN_MM, (int)TOF_MAX_MM, -8192, 8191);
    }
    int pb7 = (int)tofSmoothed >> 3;
    if (lastTofMidi < 0 || abs(pb7 - lastTofMidi) >= TOF_THRESHOLD) {
      SERIAL_MIDI.sendPitchBend(pb, config.midiChannel);
      USB_MIDI.sendPitchBend(pb, config.midiChannel);
      lastTofMidi = pb7;
    }
  } else {
    int cc;
    if (invert) {
      cc = map((int)tofSmoothed, (int)TOF_MIN_MM, (int)TOF_MAX_MM, 127, 0);
    } else {
      cc = map((int)tofSmoothed, (int)TOF_MIN_MM, (int)TOF_MAX_MM, 0, 127);
    }
    if (cc < 0) cc = 0;
    if (cc > 127) cc = 127;

    if (lastTofMidi < 0 || cc != lastTofMidi) {
      SERIAL_MIDI.sendControlChange(config.tofValue, cc, config.midiChannel);
      USB_MIDI.sendControlChange(config.tofValue, cc, config.midiChannel);
      lastTofMidi = cc;
    }
  }
}

void handleKeypadAccessKey(int keyIdx) {
  char key = KEYPAD_LABELS[keyIdx][0];

  if (key >= '0' && key <= '9') {
    if (keypadAccessLen < 3) {
      keypadAccessBuffer[keypadAccessLen++] = key;
      keypadAccessBuffer[keypadAccessLen] = '\0';
    } else {
      keypadAccessOverflow = true;
      if (Serial) {
        Serial.println("Keypad access buffer full (max 3 digits)");
      }
    }

    if (Serial) {
      Serial.print("Keypad access: ");
      Serial.println(keypadAccessBuffer);
    }
    return;
  }

  if (key == '*') {
    resetKeypadAccessBuffer();
    if (Serial) {
      Serial.println("Keypad access buffer cleared");
    }
    return;
  }

  if (key == '#') {
    if (keypadAccessLen == 0) {
      if (Serial) {
        Serial.println("Keypad access: no note entered");
      }
      return;
    }

    if (keypadAccessOverflow) {
      if (Serial) {
        Serial.println("Keypad access entry too long; cleared");
      }
      resetKeypadAccessBuffer();
      return;
    }

    int note = atoi(keypadAccessBuffer);
    if (note >= 0 && note <= 127) {
      if (Serial) {
        Serial.print("Keypad access -> Note ");
        Serial.println(note);
      }
      sendMomentaryNote((byte)note, KEYPAD_LED_COLOR);
    } else if (Serial) {
      Serial.print("Keypad access note out of range: ");
      Serial.println(note);
    }
    resetKeypadAccessBuffer();
    return;
  }

  if (Serial) {
    Serial.print("Keypad access ignored key [");
    Serial.print(key);
    Serial.println("]");
  }
}

void Scan_Keypad() {
  unsigned long currentTime = millis();

  for (int row = 0; row < 4; row++) {
    // Drive this row LOW (temporarily switch to output)
    pinMode(KEYPAD_ROW_PINS[row], OUTPUT);
    digitalWrite(KEYPAD_ROW_PINS[row], LOW);
    delayMicroseconds(10); // Let signals settle

    for (int col = 0; col < 4; col++) {
      int keyIdx = row * 4 + col;
      bool reading = digitalRead(KEYPAD_COL_PINS[col]);

      if (reading != keyRawState[keyIdx]) {
        keyDebounceTime[keyIdx] = currentTime;
      }
      keyRawState[keyIdx] = reading;

      if ((currentTime - keyDebounceTime[keyIdx]) > DEBOUNCE_DELAY) {
        if (reading == LOW && keyStableState[keyIdx] == HIGH) {
          if (config.hwMode == HW_MODE_KEYPAD_ACCESS) {
            handleKeypadAccessKey(keyIdx);
          } else {
            byte note = config.keypadNotes[keyIdx];
            if (Serial) {
              Serial.print("Key [");
              Serial.print(KEYPAD_LABELS[keyIdx]);
              Serial.print("] slot ");
              Serial.print(keyIdx + 1);
              Serial.print(" -> Note ");
              Serial.println(note);
            }
            sendMomentaryNote(note, KEYPAD_LED_COLOR);
          }
        }
        keyStableState[keyIdx] = reading;
      }
    }

    // Release row back to INPUT_PULLUP
    pinMode(KEYPAD_ROW_PINS[row], INPUT_PULLUP);
  }
}

void loop() {
  if (isKeypadHardwareMode(config.hwMode)) {
    Scan_Keypad();
  } else {
    Scan_User();
  }
  Scan_Pots();
  Scan_Sonic();
  Scan_ToF();
  updateLED();
  Process_Serial_Commands();
  USB_MIDI.read();
  SERIAL_MIDI.read();
}
