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

unsigned long lastNoteTime = 0;
const unsigned long NOTE_INTERVAL = 500; // 500ms

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
}

void Scan_User() {
  // Scan the user for button presses and potentiometers
  if (digitalRead(BUTTON_PIN1) == LOW) {
    SERIAL_MIDI.sendNoteOn(37, 127, 1);
    USB_MIDI.sendNoteOn(37, 127, 1);
    delay(5);
    SERIAL_MIDI.sendNoteOff(37, 0, 1);
    USB_MIDI.sendNoteOff(37, 0, 1);
  }
}

void loop() {

  // Read any incoming MIDI (optional, keeps MIDI library happy)
  Scan_User();
  USB_MIDI.read();
  SERIAL_MIDI.read();
}
