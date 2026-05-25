# OS Controller — RP2040 Pico Pin Map

Board: **Raspberry Pi Pico (RP2040)** — earlephilhower Arduino-Pico core

## Pin Assignment Summary

Pins 2–9 are **shared** between two mutually exclusive hardware modes selected via `/mode` and stored in EEPROM.

| GPIO | Pin Name              | Type           | Direction | Notes                                      |
|-----:|-----------------------|----------------|-----------|--------------------------------------------|
|    0 | TX0 (Serial1)         | UART TX        | Output    | MIDI TX @ 31 250 baud                      |
|    2 | BUTTON_PIN1 / COL0    | Digital        | In / I/O  | **Mode 0:** note button 1 · **Mode 1:** keypad col 0 |
|    3 | BUTTON_PIN2 / COL1    | Digital        | In / I/O  | **Mode 0:** note button 2 · **Mode 1:** keypad col 1 |
|    4 | BUTTON_PIN3 / COL2    | Digital        | In / I/O  | **Mode 0:** note button 3 · **Mode 1:** keypad col 2 |
|    5 | BUTTON_PIN4 / COL3    | Digital        | In / I/O  | **Mode 0:** note button 4 · **Mode 1:** keypad col 3 |
|    6 | BUTTON_PIN5 / ROW0    | Digital        | In / I/O  | **Mode 0:** note button 5 · **Mode 1:** keypad row 0 |
|    7 | BANK_SEL_PIN0 / ROW1  | Digital        | In / I/O  | **Mode 0:** bank bit 0   · **Mode 1:** keypad row 1  |
|    8 | BANK_SEL_PIN1 / ROW2  | Digital        | In / I/O  | **Mode 0:** bank bit 1   · **Mode 1:** keypad row 2  |
|    9 | (free) / ROW3         | Digital        | In / I/O  | **Mode 0:** unused       · **Mode 1:** keypad row 3  |
|   10 | SDA1 (Wire1)          | I2C            | Bidir     | I2C1 data                                  |
|   11 | SCL1 (Wire1)          | I2C            | Output    | I2C1 clock                                 |
|   16 | LED_PIN               | Digital        | Output    | WS2812 NeoPixel (1 LED)                    |
|   26 | A0 / POT1             | ADC            | Input     | Potentiometer 1                            |
|   27 | A1 / POT2             | ADC            | Input     | Potentiometer 2                            |
|   28 | A2 / POT3             | ADC            | Input     | Potentiometer 3                            |
|   29 | A3 / POT4             | ADC            | Input     | Potentiometer 4                            |

## Functional Groups

### Mode 0 — Button Mode (default)

#### Buttons (INPUT_PULLUP, active LOW, 50 ms debounce)

```
GPIO 2 ── BTN 1
GPIO 3 ── BTN 2
GPIO 4 ── BTN 3
GPIO 5 ── BTN 4
GPIO 6 ── BTN 5
```

#### Bank Selector (INPUT_PULLUP, active LOW)

```
GPIO 7 ── BIT 0 ─┐
GPIO 8 ── BIT 1 ─┴─ 2-bit bank select (0–3)
```

### Mode 1 — Keypad Mode (4×4 matrix)

Row pins (6-9) drive LOW one at a time (OUTPUT during scan, INPUT_PULLUP otherwise).  
Col pins (2-5) are INPUT_PULLUP; a LOW read means that key is pressed.

```
        COL0(2)  COL1(3)  COL2(4)  COL3(5)
ROW0(6)  [ 1 ]   [ 2 ]   [ 3 ]   [ A ]
ROW1(7)  [ 4 ]   [ 5 ]   [ 6 ]   [ B ]
ROW2(8)  [ 7 ]   [ 8 ]   [ 9 ]   [ C ]
ROW3(9)  [ * ]   [ 0 ]   [ # ]   [ D ]
```

Key slot = row×4 + col + 1 (1-based, used in `/k` command).  
LED flashes **cyan** for keypad presses.

### Potentiometers (ADC, 10-bit → 7-bit MIDI CC)

```
GPIO 26 (A0) ── POT 1
GPIO 27 (A1) ── POT 2
GPIO 28 (A2) ── POT 3
GPIO 29 (A3) ── POT 4
```

Smoothing: EMA alpha = 0.15, noise threshold = 4 LSB

### Serial MIDI (UART)

```
GPIO 0 ── TX  (Serial1, 31 250 baud)
```

### I2C (Wire1)

```
GPIO 10 ── SDA1 ──┬── Sonic2 (0x57) ultrasonic distance sensor
GPIO 11 ── SCL1 ──┘   (auto-detected on boot)
```

### LED

```
GPIO 16 ── WS2812 NeoPixel (1 pixel)
```

### USB (internal, no external GPIO)

- CDC Serial (debug)
- TinyUSB MIDI device

## Free GPIOs

The following RP2040 Pico GPIOs are **not used** and available for expansion:

> **Note:** GPIO 9 is now used by the keypad (COL3) in Mode 1. It is only free in Mode 0.

| GPIO | Pico Pin | Capabilities                      | Available        |
|-----:|---------:|-----------------------------------|------------------|
|    1 |        2 | UART0 RX, SPI0 CS, I2C0 SDA, PWM | Always           |
|    9 |       12 | SPI1 CS, I2C0 SCL, PWM            | Mode 0 only      |
|   12 |       16 | SPI1 RX, I2C0 SDA, PWM            | Always           |
|   13 |       17 | SPI1 CS, I2C0 SCL, PWM            | Always           |
|   14 |       19 | SPI1 SCK, I2C1 SDA, PWM           | Always           |
|   15 |       20 | SPI1 TX, I2C1 SCL, PWM            | Always           |
|   17 |       22 | SPI0 CS, I2C0 SCL, PWM            | Always           |
|   18 |       24 | SPI0 SCK, I2C1 SDA, PWM           | Always           |
|   19 |       25 | SPI0 TX, I2C1 SCL, PWM            | Always           |
|   20 |       26 | SPI0 RX, I2C0 SDA, PWM            | Always           |
|   21 |       27 | SPI0 CS, I2C0 SCL, PWM            | Always           |
|   22 |       29 | SPI0 SCK, I2C1 SDA, PWM           | Always           |

> **Note:** Encoders will use I2C breakout boards on Wire1, not dedicated GPIO pins. `config_eeprom.h` reserves EEPROM layout for 4 encoders (`MAX_ENCODERS = 4`). The Sonic2 distance sensor is the first I2C device supported (address 0x57).
