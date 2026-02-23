/*
 * OS MIDI Controller
 * Outputs on USB MIDI and Serial MIDI (TX0)
 * Configuration stored in RP2040 emulated EEPROM (flash)
 */

#define PICO_VERSION "v1.1"

#include "config_eeprom.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>
#include <MIDI.h>
#include <Wire.h>

// Forward declarations
uint16_t calculateChecksum();
void resetConfigToDefaults();
void saveConfig();
bool loadConfig();
int readBank();

// 5 note buttons (pins 2-6)
#define BUTTON_PIN1 2
#define BUTTON_PIN2 3
#define BUTTON_PIN3 4
#define BUTTON_PIN4 5
#define BUTTON_PIN5 6

// 2 bank selector pins (binary: 4 banks)
// Both HIGH (released) = Bank 0, pin7 LOW = +1, pin8 LOW = +2
#define BANK_SEL_PIN0 7  // Bit 0
#define BANK_SEL_PIN1 8  // Bit 1

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

// Potentiometer state
const int POT_PINS[MAX_POTS] = {A0, A1, A2, A3};
int potSmoothed[MAX_POTS] = {0, 0, 0, 0};   // Smoothed ADC value (0-1023)
int lastMidiValue[MAX_POTS] = {-1, -1, -1, -1}; // Last sent MIDI value (-1 = never sent)
const int POT_THRESHOLD = 4;  // ADC noise threshold (prevents jitter, ~0.4%)
const float POT_ALPHA = 0.15; // Smoothing factor (0.0=slow, 1.0=no smoothing)
bool adcDebugMode = false;    // Toggle with /a command
unsigned long lastAdcPrint = 0;
const unsigned long ADC_PRINT_INTERVAL = 200; // Print every 200ms

// Configuration in RAM (current working config)
struct Config {
  byte buttonNotes[MAX_BUTTONS];
  byte potTypes[MAX_POTS];
  byte potValues[MAX_POTS];
  byte encCCs[MAX_ENCODERS];
  byte encModes[MAX_ENCODERS];
} config;

// Read bank from selector pins (active LOW with pull-ups)
// Pin 7 = bit 0, Pin 8 = bit 1
// Both released (HIGH) = Bank 0
// Pin 7 pressed (LOW)  = Bank 1
// Pin 8 pressed (LOW)  = Bank 2
// Both pressed (LOW)   = Bank 3
int readBank() {
  int bit0 = (digitalRead(BANK_SEL_PIN0) == LOW) ? 1 : 0;
  int bit1 = (digitalRead(BANK_SEL_PIN1) == LOW) ? 1 : 0;
  return bit0 | (bit1 << 1);
}

void setup() {
  // Initialize 5 note buttons
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(BUTTON_PIN4, INPUT_PULLUP);
  pinMode(BUTTON_PIN5, INPUT_PULLUP);

  // Initialize bank selector pins (active LOW)
  pinMode(BANK_SEL_PIN0, INPUT_PULLUP);
  pinMode(BANK_SEL_PIN1, INPUT_PULLUP);

  // Initialize built-in RGB LED
  led.begin();
  led.setBrightness(30); // Keep it dim to not blind you
  led.clear();
  led.show();

  // Initialize button states and debounce times
  unsigned long initTime = millis();
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
  lastDebounceTime1 = initTime;
  lastDebounceTime2 = initTime;
  lastDebounceTime3 = initTime;
  lastDebounceTime4 = initTime;
  lastDebounceTime5 = initTime;

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
  Serial.println("5 Note buttons + 2 Bank selectors (4 banks)");
  Serial.println("USB MIDI: Active");
  Serial.println("Serial MIDI (TX0): Active");
  Serial.println("EEPROM: Emulated (flash)");

  // Show current bank at boot
  int bank = readBank();
  Serial.print("Current Bank: ");
  Serial.println(bank);
  Serial.println("Type /h for help\n");

  // Auto-load configuration from EEPROM
  if (!loadConfig()) {
    Serial.println("No valid config in EEPROM, using factory defaults");
    resetConfigToDefaults();
  }

  Serial.println("Ready!\n");
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

  // Copy default mappings to RAM
  for (int i = 0; i < MAX_BUTTONS; i++) {
    config.buttonNotes[i] = DEFAULT_BUTTON_NOTES[i];
  }
  for (int i = 0; i < MAX_POTS; i++) {
    config.potTypes[i] = DEFAULT_POT_TYPES[i];
    config.potValues[i] = DEFAULT_POT_VALUES[i];
  }
  for (int i = 0; i < MAX_ENCODERS; i++) {
    config.encCCs[i] = DEFAULT_ENC_CCS[i];
    config.encModes[i] = DEFAULT_ENC_MODES[i];
  }

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
  EEPROM.write(EEPROM_ADDR_FLAGS, 0);
  EEPROM.write(EEPROM_ADDR_NUM_BTNS, MAX_BUTTONS);
  EEPROM.write(EEPROM_ADDR_NUM_POTS, MAX_POTS);
  EEPROM.write(EEPROM_ADDR_NUM_ENCS, MAX_ENCODERS);

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

  Serial.println("Configuration loaded successfully!");
  Serial.println("====================\n");
  return true;
}

void printHelp() {
  Serial.println("\n=== Command Help ===");
  Serial.println("MAPPING:");
  Serial.println("  /b <1-20> <0-127>  - Map button slot to MIDI note");
  Serial.println("     Slots: Bank0=1-5, Bank1=6-10, Bank2=11-15, Bank3=16-20");
  Serial.println("  /p <1-4> cc <0-127> - Map pot to CC number");
  Serial.println("  /p <1-4> pb        - Map pot to Pitch Bend");
  Serial.println("  /m                 - Show all current mappings");
  Serial.println("");
  Serial.println("STORAGE:");
  Serial.println("  /save   - Save configuration to EEPROM");
  Serial.println("  /load   - Load configuration from EEPROM");
  Serial.println("  /reset  - Reset to factory defaults");
  Serial.println("");
  Serial.println("INFO:");
  Serial.println("  /h - Show this help");
  Serial.println("  /d - Dump EEPROM contents");
  Serial.println("  /test - Test button/bank states");
  Serial.println("  /a - Toggle ADC debug (raw pot values)");
  Serial.println("  /i - Scan I2C bus (Wire1: GP10/GP11)");
  Serial.println("====================\n");
}

void printMidiMapping() {
  Serial.println("\n=== Current MIDI Mapping ===");

  int currentBank = readBank();
  Serial.print("Active Bank: ");
  Serial.print(currentBank);
  Serial.print(" (Pin7=");
  Serial.print(digitalRead(BANK_SEL_PIN0) == LOW ? "LOW" : "HIGH");
  Serial.print(", Pin8=");
  Serial.print(digitalRead(BANK_SEL_PIN1) == LOW ? "LOW" : "HIGH");
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

  Serial.println("\nPOTENTIOMETERS:");
  for (int i = 0; i < MAX_POTS; i++) {
    Serial.print("  Pot ");
    Serial.print(i + 1);
    Serial.print(" (A");
    Serial.print(i);
    Serial.print(") -> ");

    if (config.potTypes[i] == POT_TYPE_PITCHBEND) {
      Serial.println("Pitch Bend");
    } else {
      Serial.print("CC ");
      Serial.println(config.potValues[i]);
    }
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

  for (byte addr = 1; addr < 127; addr++) {
    Wire1.beginTransmission(addr);
    byte error = Wire1.endTransmission();

    if (error == 0) {
      Serial.print("  0x");
      if (addr < 0x10) Serial.print("0");
      Serial.print(addr, HEX);
      Serial.print(" (");
      Serial.print(addr);
      Serial.println(") - found");
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
      // Test button and bank states
      int bank = readBank();
      int offset = bank * NUM_NOTE_BUTTONS;
      Serial.println("\n=== Button/Bank Test ===");
      Serial.print("Bank Sel Pin7: ");
      Serial.println(digitalRead(BANK_SEL_PIN0) == LOW ? "LOW (1)" : "HIGH (0)");
      Serial.print("Bank Sel Pin8: ");
      Serial.println(digitalRead(BANK_SEL_PIN1) == LOW ? "LOW (1)" : "HIGH (0)");
      Serial.print("Active Bank: ");
      Serial.println(bank);
      Serial.println("");
      for (int i = 0; i < NUM_NOTE_BUTTONS; i++) {
        int pin = BUTTON_PIN1 + i;
        Serial.print("Btn ");
        Serial.print(i + 1);
        Serial.print(" (pin ");
        Serial.print(pin);
        Serial.print("): ");
        Serial.print(digitalRead(pin) == LOW ? "PRESSED" : "RELEASED");
        Serial.print("  -> Note ");
        Serial.println(config.buttonNotes[offset + i]);
      }
      Serial.println("==================\n");
    } else if (command == "/a") {
      adcDebugMode = !adcDebugMode;
      Serial.print("ADC debug: ");
      Serial.println(adcDebugMode ? "ON" : "OFF");
    } else if (command == "/i") {
      scanI2C();
    } else if (command.length() > 0) {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.println("Type /h for help");
    }
  }
}

// Flash the LED with the current bank color (non-blocking)
void flashLED(int bank) {
  uint32_t color = BANK_COLORS[bank];
  led.setPixelColor(0, color);
  led.show();
  ledOnTime = millis();
  ledActive = true;
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
        flashLED(bank); // Flash LED with bank color
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
        SERIAL_MIDI.sendNoteOn(note, 127, 1);
        USB_MIDI.sendNoteOn(note, 127, 1);
        delay(5);
        SERIAL_MIDI.sendNoteOff(note, 0, 1);
        USB_MIDI.sendNoteOff(note, 0, 1);
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
        SERIAL_MIDI.sendPitchBend(pb, 1);
        USB_MIDI.sendPitchBend(pb, 1);
        lastMidiValue[i] = pb7;
      }
    } else {
      // CC: map 0-1023 -> 0-127
      int cc = smoothed >> 3; // Fast divide by 8 (1024/128 = 8)
      if (cc > 127) cc = 127;

      if (lastMidiValue[i] < 0 || cc != lastMidiValue[i]) {
        byte ccNum = config.potValues[i];
        SERIAL_MIDI.sendControlChange(ccNum, cc, 1);
        USB_MIDI.sendControlChange(ccNum, cc, 1);
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

void loop() {
  Scan_User();
  Scan_Pots();
  updateLED(); // Non-blocking LED flash timeout
  Process_Serial_Commands();
  USB_MIDI.read();
  SERIAL_MIDI.read();
}
