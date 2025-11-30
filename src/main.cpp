/*
 * OS MIDI Controller
 * Outputs on USB MIDI and Serial MIDI (TX0)
 * Configuration stored in RP2040 emulated EEPROM (flash)
 */

#define PICO_VERSION "v1.1"

#include "config_eeprom.h"
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>
#include <MIDI.h>

// Forward declarations
uint16_t calculateChecksum();
void resetConfigToDefaults();
void saveConfig();
bool loadConfig();

#define BUTTON_PIN1 2
#define BUTTON_PIN2 3
#define BUTTON_PIN3 4
#define BUTTON_PIN4 5
#define BUTTON_PIN5 6
#define BUTTON_PIN6 7
#define BUTTON_PIN7 8

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

// Debounce variables for all buttons
unsigned long lastPressTime1 = 0;
unsigned long lastPressTime2 = 0;
unsigned long lastPressTime3 = 0;
unsigned long lastPressTime4 = 0;
unsigned long lastPressTime5 = 0;
unsigned long lastPressTime6 = 0;
unsigned long lastPressTime7 = 0;
const unsigned long DEBOUNCE_DELAY = 100; // 100ms debounce

// Configuration in RAM (current working config)
struct Config {
  byte buttonNotes[MAX_BUTTONS];
  byte potTypes[MAX_POTS];
  byte potValues[MAX_POTS];
  byte encCCs[MAX_ENCODERS];
  byte encModes[MAX_ENCODERS];
} config;

void setup() {
  // Initialize buttons first
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(BUTTON_PIN4, INPUT_PULLUP);
  pinMode(BUTTON_PIN5, INPUT_PULLUP);
  pinMode(BUTTON_PIN6, INPUT_PULLUP);
  pinMode(BUTTON_PIN7, INPUT_PULLUP);

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
  Serial.println("7 Buttons initialized");
  Serial.println("USB MIDI: Active");
  Serial.println("Serial MIDI (TX0): Active");
  Serial.println("EEPROM: Emulated (flash)");
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
  Serial.println("  /b <1-16> <0-127>  - Map button to MIDI note");
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
  Serial.println("====================\n");
}

void printMidiMapping() {
  Serial.println("\n=== Current MIDI Mapping ===");

  Serial.println("\nBUTTONS:");
  for (int i = 0; i < 7; i++) { // Only show physical buttons 1-7
    Serial.print("  Button ");
    Serial.print(i + 1);
    Serial.print(" (GPIO");
    Serial.print(BUTTON_PIN1 + i);
    Serial.print(") -> Note ");
    Serial.println(config.buttonNotes[i]);
  }
  if (MAX_BUTTONS > 7) {
    Serial.println("  Buttons 8-16: Reserved for future expansion");
  }

  Serial.println("\nPOTENTIOMETERS:");
  for (int i = 0; i < MAX_POTS; i++) {
    Serial.print("  Pot ");
    Serial.print(i + 1);
    Serial.print(" (A");
    Serial.print(i);
    Serial.print(") -> ");

    if (config.potTypes[i] == POT_TYPE_PITCHBEND) {
      Serial.println("Pitch Bend ★");
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
    } else if (command.length() > 0) {
      Serial.print("Unknown command: ");
      Serial.println(command);
      Serial.println("Type /h for help");
    }
  }
}

void Scan_User() {
  unsigned long currentTime = millis();

  // Button 1 - Uses config
  if (digitalRead(BUTTON_PIN1) == LOW &&
      (currentTime - lastPressTime1) > DEBOUNCE_DELAY) {
    lastPressTime1 = currentTime;
    byte note = config.buttonNotes[0];
    if (Serial) {
      Serial.print("Button 1: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }

  // Button 2 - Uses config
  if (digitalRead(BUTTON_PIN2) == LOW &&
      (currentTime - lastPressTime2) > DEBOUNCE_DELAY) {
    lastPressTime2 = currentTime;
    byte note = config.buttonNotes[1];
    if (Serial) {
      Serial.print("Button 2: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }

  // Button 3 - Uses config
  if (digitalRead(BUTTON_PIN3) == LOW &&
      (currentTime - lastPressTime3) > DEBOUNCE_DELAY) {
    lastPressTime3 = currentTime;
    byte note = config.buttonNotes[2];
    if (Serial) {
      Serial.print("Button 3: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }

  // Button 4 - Uses config
  if (digitalRead(BUTTON_PIN4) == LOW &&
      (currentTime - lastPressTime4) > DEBOUNCE_DELAY) {
    lastPressTime4 = currentTime;
    byte note = config.buttonNotes[3];
    if (Serial) {
      Serial.print("Button 4: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }

  // Button 5 - Uses config
  if (digitalRead(BUTTON_PIN5) == LOW &&
      (currentTime - lastPressTime5) > DEBOUNCE_DELAY) {
    lastPressTime5 = currentTime;
    byte note = config.buttonNotes[4];
    if (Serial) {
      Serial.print("Button 5: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }

  // Button 6 - Uses config
  if (digitalRead(BUTTON_PIN6) == LOW &&
      (currentTime - lastPressTime6) > DEBOUNCE_DELAY) {
    lastPressTime6 = currentTime;
    byte note = config.buttonNotes[5];
    if (Serial) {
      Serial.print("Button 6: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }

  // Button 7 - Uses config
  if (digitalRead(BUTTON_PIN7) == LOW &&
      (currentTime - lastPressTime7) > DEBOUNCE_DELAY) {
    lastPressTime7 = currentTime;
    byte note = config.buttonNotes[6];
    if (Serial) {
      Serial.print("Button 7: Note ");
      Serial.println(note);
    }
    SERIAL_MIDI.sendNoteOn(note, 127, 1);
    USB_MIDI.sendNoteOn(note, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(note, 0, 1);
    USB_MIDI.sendNoteOff(note, 0, 1);
  }
}

void loop() {
  // Read any incoming MIDI (optional, keeps MIDI library happy)
  Scan_User();
  Process_Serial_Commands();
  USB_MIDI.read();
  SERIAL_MIDI.read();
}
