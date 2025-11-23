/*
 * MIDI Test - Note 36 every 500ms
 * Outputs on USB MIDI and Serial MIDI (TX0)
 */

#define PICO_VERSION "v1.0"

#include <Adafruit_TinyUSB.h>
#include <MIDI.h>

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

void setup() {

  // Initialize buttons
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(BUTTON_PIN4, INPUT_PULLUP);
  pinMode(BUTTON_PIN5, INPUT_PULLUP);
  pinMode(BUTTON_PIN6, INPUT_PULLUP);
  pinMode(BUTTON_PIN7, INPUT_PULLUP);

  // Initialize USB MIDI
  TinyUSB_Device_Init(0);
  USBDevice.setManufacturerDescriptor("OS MIDI Controller");
  USBDevice.setProductDescriptor("MIDI Test");

  // Start USB MIDI
  USB_MIDI.begin(MIDI_CHANNEL_OMNI);

  // Start Hardware Serial MIDI on TX0 (GPIO0) at standard MIDI baud rate
  Serial1.setTX(0); // TX0 = GPIO0
  SERIAL_MIDI.begin(MIDI_CHANNEL_OMNI);

  // Enable core Serial (CDC) for debugging
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== OS MIDI Controller ===");
  Serial.println("Version: " PICO_VERSION);
  Serial.println("7 Buttons initialized");
  Serial.println("USB MIDI: Active");
  Serial.println("Serial MIDI (TX0): Active");
  Serial.println("Ready!\n");
}

void Scan_User() {
  unsigned long currentTime = millis();

  // Button 1 - MIDI Note 36 (Kick)
  if (digitalRead(BUTTON_PIN1) == LOW &&
      (currentTime - lastPressTime1) > DEBOUNCE_DELAY) {
    lastPressTime1 = currentTime;
    Serial.println("Button 1: Note 36 (Kick)");
    SERIAL_MIDI.sendNoteOn(36, 127, 1);
    USB_MIDI.sendNoteOn(36, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(36, 0, 1);
    USB_MIDI.sendNoteOff(36, 0, 1);
  }

  // Button 2 - MIDI Note 37 (Snare Side Stick)
  if (digitalRead(BUTTON_PIN2) == LOW &&
      (currentTime - lastPressTime2) > DEBOUNCE_DELAY) {
    lastPressTime2 = currentTime;
    Serial.println("Button 2: Note 37 (Side Stick)");
    SERIAL_MIDI.sendNoteOn(37, 127, 1);
    USB_MIDI.sendNoteOn(37, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(37, 0, 1);
    USB_MIDI.sendNoteOff(37, 0, 1);
  }

  // Button 3 - MIDI Note 38 (Snare)
  if (digitalRead(BUTTON_PIN3) == LOW &&
      (currentTime - lastPressTime3) > DEBOUNCE_DELAY) {
    lastPressTime3 = currentTime;
    Serial.println("Button 3: Note 38 (Snare)");
    SERIAL_MIDI.sendNoteOn(38, 127, 1);
    USB_MIDI.sendNoteOn(38, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(38, 0, 1);
    USB_MIDI.sendNoteOff(38, 0, 1);
  }

  // Button 4 - MIDI Note 39 (Clap)
  if (digitalRead(BUTTON_PIN4) == LOW &&
      (currentTime - lastPressTime4) > DEBOUNCE_DELAY) {
    lastPressTime4 = currentTime;
    Serial.println("Button 4: Note 39 (Clap)");
    SERIAL_MIDI.sendNoteOn(39, 127, 1);
    USB_MIDI.sendNoteOn(39, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(39, 0, 1);
    USB_MIDI.sendNoteOff(39, 0, 1);
  }

  // Button 5 - MIDI Note 40 (Electric Snare)
  if (digitalRead(BUTTON_PIN5) == LOW &&
      (currentTime - lastPressTime5) > DEBOUNCE_DELAY) {
    lastPressTime5 = currentTime;
    Serial.println("Button 5: Note 40 (Electric Snare)");
    SERIAL_MIDI.sendNoteOn(40, 127, 1);
    USB_MIDI.sendNoteOn(40, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(40, 0, 1);
    USB_MIDI.sendNoteOff(40, 0, 1);
  }

  // Button 6 - MIDI Note 41 (Low Floor Tom)
  if (digitalRead(BUTTON_PIN6) == LOW &&
      (currentTime - lastPressTime6) > DEBOUNCE_DELAY) {
    lastPressTime6 = currentTime;
    Serial.println("Button 6: Note 41 (Low Floor Tom)");
    SERIAL_MIDI.sendNoteOn(41, 127, 1);
    USB_MIDI.sendNoteOn(41, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(41, 0, 1);
    USB_MIDI.sendNoteOff(41, 0, 1);
  }

  // Button 7 - MIDI Note 42 (Closed Hi-Hat)
  if (digitalRead(BUTTON_PIN7) == LOW &&
      (currentTime - lastPressTime7) > DEBOUNCE_DELAY) {
    lastPressTime7 = currentTime;
    Serial.println("Button 7: Note 42 (Closed Hi-Hat)");
    SERIAL_MIDI.sendNoteOn(42, 127, 1);
    USB_MIDI.sendNoteOn(42, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(42, 0, 1);
    USB_MIDI.sendNoteOff(42, 0, 1);
  }
}

void loop() {

  // Read any incoming MIDI (optional, keeps MIDI library happy)
  Scan_User();
  USB_MIDI.read();
  SERIAL_MIDI.read();
}
