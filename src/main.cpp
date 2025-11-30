/*
 * MIDI Test - Note 36 every 500ms
 * Outputs on USB MIDI and Serial MIDI (TX0)
 */

#define PICO_VERSION "v1.0"

#include "config_eeprom.h"
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include <Wire.h>

// Forward declarations
void initI2C();
byte readEEPROM(uint16_t address);
void writeEEPROM(uint16_t address, byte data);
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

// I2C EEPROM 24LC16 (2KB)
// IMPORTANT: Pins A0, A1, A2 MUST be tied to GND (not floating!)
// Floating address pins will cause multiple addresses to respond (0x50-0x57)
#define EEPROM_I2C_ADDRESS 0x50 // Base address with A0=A1=A2=GND
#define EEPROM_SIZE 2048        // 2KB = 2048 bytes
#define I2C_SDA_PIN 14          // SDA1 on RP2040 Zero (GPIO14)
#define I2C_SCL_PIN 15          // SCL1 on RP2040 Zero (GPIO15)

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

// I2C initialization flag
bool i2cInitialized = false;

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

  // NOTE: I2C is initialized on-demand when first I2C command is used
  // This prevents USB enumeration issues

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
  Serial.println("I2C: GPIO14/15 (initialized on demand)");
  Serial.println("Type /h for help\n");

  // Auto-load configuration from EEPROM
  if (!loadConfig()) {
    Serial.println("No valid config in EEPROM, using factory defaults");
    resetConfigToDefaults();
  }

  Serial.println("Ready!\n");
}

void initI2C() {
  if (!i2cInitialized) {
    Serial.println("Initializing I2C1...");
    Wire1.setSDA(I2C_SDA_PIN); // GPIO14
    Wire1.setSCL(I2C_SCL_PIN); // GPIO15
    Wire1.begin();
    Wire1.setClock(100000); // 100kHz for EEPROM
    delay(50);
    i2cInitialized = true;
    Serial.println("I2C1 initialized on GPIO14/15");
  }
}

// ============================================================================
// EEPROM READ/WRITE FUNCTIONS (Must be before config functions)
// ============================================================================

byte readEEPROM(uint16_t address) {
  byte blockBits = (address >> 8) & 0x07;           // Extract bits 10:8
  byte deviceAddr = EEPROM_I2C_ADDRESS | blockBits; // Add block to base address
  byte wordAddr = address & 0xFF;                   // Lower 8 bits

  Wire1.beginTransmission(deviceAddr);
  Wire1.write(wordAddr); // Only send 8-bit word address
  Wire1.endTransmission();

  Wire1.requestFrom(deviceAddr, 1);
  if (Wire1.available()) {
    return Wire1.read();
  }
  return 0xFF;
}

void writeEEPROM(uint16_t address, byte data) {
  byte blockBits = (address >> 8) & 0x07;           // Extract bits 10:8
  byte deviceAddr = EEPROM_I2C_ADDRESS | blockBits; // Add block to base address
  byte wordAddr = address & 0xFF;                   // Lower 8 bits

  Wire1.beginTransmission(deviceAddr);
  Wire1.write(wordAddr); // Only send 8-bit word address
  Wire1.write(data);
  byte error = Wire1.endTransmission();

  if (error != 0) {
    Serial.print("Write error: ");
    Serial.println(error);
  }

  delay(5); // Write cycle time (5ms typical for 24LC16)
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

uint16_t calculateChecksum() {
  uint16_t sum = 0;
  for (uint16_t addr = 0; addr < EEPROM_ADDR_CHECKSUM; addr++) {
    sum += readEEPROM(addr);
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
  initI2C();

  Serial.println("\n=== Saving Configuration to EEPROM ===");

  // Write header
  writeEEPROM(EEPROM_ADDR_MAGIC + 0, CONFIG_MAGIC_0);
  writeEEPROM(EEPROM_ADDR_MAGIC + 1, CONFIG_MAGIC_1);
  writeEEPROM(EEPROM_ADDR_MAGIC + 2, CONFIG_MAGIC_2);
  writeEEPROM(EEPROM_ADDR_MAGIC + 3, CONFIG_MAGIC_3);
  writeEEPROM(EEPROM_ADDR_VERSION, CONFIG_VERSION);
  writeEEPROM(EEPROM_ADDR_FLAGS, 0);
  writeEEPROM(EEPROM_ADDR_NUM_BTNS, MAX_BUTTONS);
  writeEEPROM(EEPROM_ADDR_NUM_POTS, MAX_POTS);
  writeEEPROM(EEPROM_ADDR_NUM_ENCS, MAX_ENCODERS);

  // Write button mappings
  for (int i = 0; i < MAX_BUTTONS; i++) {
    writeEEPROM(EEPROM_ADDR_BUTTONS + i, config.buttonNotes[i]);
  }

  // Write pot mappings
  for (int i = 0; i < MAX_POTS; i++) {
    writeEEPROM(EEPROM_ADDR_POTS + (i * 2), config.potTypes[i]);
    writeEEPROM(EEPROM_ADDR_POTS + (i * 2) + 1, config.potValues[i]);
  }

  // Write encoder mappings
  for (int i = 0; i < MAX_ENCODERS; i++) {
    writeEEPROM(EEPROM_ADDR_ENCODERS + (i * 2), config.encCCs[i]);
    writeEEPROM(EEPROM_ADDR_ENCODERS + (i * 2) + 1, config.encModes[i]);
  }

  // Calculate and write checksum
  uint16_t checksum = calculateChecksum();
  writeEEPROM(EEPROM_ADDR_CHECKSUM, (byte)(checksum >> 8));
  writeEEPROM(EEPROM_ADDR_CHECKSUM + 1, (byte)(checksum & 0xFF));

  Serial.println("Configuration saved successfully!");
  Serial.println("====================\n");
}

bool loadConfig() {
  initI2C();

  Serial.println("\n=== Loading Configuration from EEPROM ===");

  // Check magic bytes
  if (readEEPROM(EEPROM_ADDR_MAGIC + 0) != CONFIG_MAGIC_0 ||
      readEEPROM(EEPROM_ADDR_MAGIC + 1) != CONFIG_MAGIC_1 ||
      readEEPROM(EEPROM_ADDR_MAGIC + 2) != CONFIG_MAGIC_2 ||
      readEEPROM(EEPROM_ADDR_MAGIC + 3) != CONFIG_MAGIC_3) {
    Serial.println("EEPROM not initialized (magic bytes missing)");
    return false;
  }

  // Check version
  byte version = readEEPROM(EEPROM_ADDR_VERSION);
  if (version != CONFIG_VERSION) {
    Serial.print("Version mismatch: ");
    Serial.println(version);
    return false;
  }

  // Verify checksum
  uint16_t storedChecksum = (readEEPROM(EEPROM_ADDR_CHECKSUM) << 8) |
                            readEEPROM(EEPROM_ADDR_CHECKSUM + 1);
  uint16_t calcChecksum = calculateChecksum();

  if (storedChecksum != calcChecksum) {
    Serial.println("Checksum mismatch! EEPROM may be corrupted");
    return false;
  }

  // Load button mappings
  for (int i = 0; i < MAX_BUTTONS; i++) {
    config.buttonNotes[i] = readEEPROM(EEPROM_ADDR_BUTTONS + i);
  }

  // Load pot mappings
  for (int i = 0; i < MAX_POTS; i++) {
    config.potTypes[i] = readEEPROM(EEPROM_ADDR_POTS + (i * 2));
    config.potValues[i] = readEEPROM(EEPROM_ADDR_POTS + (i * 2) + 1);
  }

  // Load encoder mappings
  for (int i = 0; i < MAX_ENCODERS; i++) {
    config.encCCs[i] = readEEPROM(EEPROM_ADDR_ENCODERS + (i * 2));
    config.encModes[i] = readEEPROM(EEPROM_ADDR_ENCODERS + (i * 2) + 1);
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
  Serial.println("  /i - Scan I2C bus");
  Serial.println("  /d - Dump EEPROM");
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

void scanI2C() {
  initI2C(); // Initialize I2C on first use

  Serial.println("\n=== I2C Bus Scanner ===");
  Serial.println("Scanning I2C1 bus (0x00 to 0x7F)...\n");

  int devicesFound = 0;

  for (byte address = 0; address < 128; address++) {
    Wire1.beginTransmission(address);
    byte error = Wire1.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(" (");
      Serial.print(address);
      Serial.println(")");

      // Identify known devices
      if (address == 0x50) {
        Serial.println("  -> 24LC16 EEPROM (or similar)");
      }

      devicesFound++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  Serial.println();
  if (devicesFound == 0) {
    Serial.println("No I2C devices found!");
    Serial.println("Check wiring and pull-up resistors.");
  } else {
    Serial.print("Scan complete. Found ");
    Serial.print(devicesFound);
    Serial.println(" device(s).");
  }
  Serial.println("====================\n");
}

// 24LC16 uses block addressing: 8 blocks of 256 bytes
// Block select bits (address bits 10:8) go into the device address
// Device address format: 1010 B2 B1 B0 R/W
void writeStringEEPROM(uint16_t startAddress, String text) {
  initI2C();

  Serial.print("Writing \"");
  Serial.print(text);
  Serial.println("\" to EEPROM...");

  // Write string length first
  byte len = text.length();
  if (len > 255)
    len = 255; // Max 255 chars

  writeEEPROM(startAddress, len);

  // Write each character
  for (int i = 0; i < len; i++) {
    writeEEPROM(startAddress + 1 + i, text[i]);
    if (i % 16 == 0 && i > 0) {
      Serial.print(".");
    }
  }

  Serial.println("\nWrite complete!");
  Serial.print("Stored ");
  Serial.print(len);
  Serial.println(" bytes at address 0x0000");
}

void readStringEEPROM(uint16_t startAddress) {
  initI2C();

  Serial.println("\n=== Reading String from EEPROM ===");

  // Read length
  byte len = readEEPROM(startAddress);

  if (len == 0 || len == 0xFF) {
    Serial.println("No string found (empty or uninitialized)");
    Serial.println("====================\n");
    return;
  }

  Serial.print("Length: ");
  Serial.print(len);
  Serial.println(" bytes");
  Serial.print("String: \"");

  // Read and print each character
  for (int i = 0; i < len; i++) {
    byte ch = readEEPROM(startAddress + 1 + i);
    if (ch >= 32 && ch <= 126) {
      Serial.print((char)ch);
    } else {
      Serial.print("?");
    }
  }

  Serial.println("\"");
  Serial.println("====================\n");
}

void dumpEEPROM() {
  initI2C(); // Initialize I2C on first use

  Serial.println("\n=== EEPROM Dump (24LC16, 2KB) ===");
  Serial.println("Address: 0x50 | Size: 2048 bytes\n");

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
      data[i] = readEEPROM(addr + i);
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
    } else if (command == "/i") {
      scanI2C();
    } else if (command == "/r") {
      readStringEEPROM(0);
    } else if (command == "/d") {
      dumpEEPROM();
    } else if (command.startsWith("/s ")) {
      // Extract text after "/s "
      String text = command.substring(3);
      if (text.length() > 0) {
        writeStringEEPROM(0, text);
      } else {
        Serial.println("Usage: /s <text>");
      }
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
