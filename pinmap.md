# Teletype firmware GPIO map

Target: **RP2040**, Arduino-Pico core, PlatformIO `pico` environment. These are **GPIO numbers**, not connector positions or physical pin numbers. The build profile does not define the controller's physical board layout.

| GPIO | Firmware name | Function |
| ---: | --- | --- |
| 0 | Serial1 TX | Hardware MIDI output at 31,250 baud |
| 2 | `BUTTON_PIN1` | Note button 1 |
| 3 | `BUTTON_PIN2` | Note button 2 |
| 4 | `BUTTON_PIN3` | Note button 3 |
| 5 | `BUTTON_PIN4` | Note button 4 |
| 6 | `BUTTON_PIN5` | Note button 5 |
| 7 | `BANK_SEL_PIN1` | Bank selector bit 1 |
| 8 | `BANK_SEL_PIN0` | Bank selector bit 0 |
| 16 | `LED_PIN` | One WS2812 NeoPixel, GRB at 800 kHz |
| 26 | A0 | Pot 1 |
| 27 | A1 | Pot 2 |
| 28 | A2 | Pot 3 |
| 29 | A3 | Pot 4 |

## Buttons and bank selector

Buttons and bank inputs use `INPUT_PULLUP`. Buttons are active LOW, with a 50 ms debounce interval. Each press sends Note On at velocity 127 and Note Off about 5 ms later.

The selector is read as `bank = bit0 | (bit1 << 1)`, with HIGH representing 1:

| GPIO 7 | GPIO 8 | Bank | Note slots | LED flash |
| --- | --- | ---: | --- | --- |
| LOW | LOW | 0 | 1–5 | Red |
| LOW | HIGH | 1 | 6–10 | Green |
| HIGH | LOW | 2 | 11–15 | Blue |
| HIGH | HIGH | 3 | 16–20 | White |

The LED flashes for approximately 100 ms on a note trigger.

## Potentiometers

The firmware uses 10-bit ADC readings and exponential smoothing with alpha 0.15. CC is sent when the smoothed 7-bit value changes. Pitch bend spans −8192 to +8191, with updates also gated by changes in the smoothed 7-bit value. Muted pots are skipped.

## MIDI and USB

Notes, CC and pitch bend are sent to both USB MIDI and Serial1 TX on GPIO 0. Configuration requests and replies use **USB MIDI SysEx only**.

USB CDC is enabled for bootloader reset/upload support. The firmware does not implement a serial text-command interface.

Source: [`src/main.cpp`](src/main.cpp), [`src/config_eeprom.h`](src/config_eeprom.h) and [`platformio.ini`](platformio.ini).
