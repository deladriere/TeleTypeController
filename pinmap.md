# OS Controller — RP2040 Pico Pin Map

Board: **Raspberry Pi Pico (RP2040)** — earlephilhower Arduino-Pico core

## Pin Assignment Summary

| GPIO | Pin Name          | Type           | Direction | Notes                     |
|-----:|-------------------|----------------|-----------|---------------------------|
|    0 | TX0 (Serial1)     | UART TX        | Output    | MIDI TX @ 31 250 baud     |
|    2 | BUTTON_PIN1       | Digital        | Input     | Note button 1, pull-up    |
|    3 | BUTTON_PIN2       | Digital        | Input     | Note button 2, pull-up    |
|    4 | BUTTON_PIN3       | Digital        | Input     | Note button 3, pull-up    |
|    5 | BUTTON_PIN4       | Digital        | Input     | Note button 4, pull-up    |
|    6 | BUTTON_PIN5       | Digital        | Input     | Note button 5, pull-up    |
|    7 | BANK_SEL_PIN0     | Digital        | Input     | Bank selector bit 0, pull-up (active LOW) |
|    8 | BANK_SEL_PIN1     | Digital        | Input     | Bank selector bit 1, pull-up (active LOW) |
|   10 | SDA1 (Wire1)      | I2C            | Bidir     | I2C1 data                 |
|   11 | SCL1 (Wire1)      | I2C            | Output    | I2C1 clock                |
|   16 | LED_PIN           | Digital        | Output    | WS2812 NeoPixel (1 LED)   |
|   26 | A0 / POT1         | ADC            | Input     | Potentiometer 1           |
|   27 | A1 / POT2         | ADC            | Input     | Potentiometer 2           |
|   28 | A2 / POT3         | ADC            | Input     | Potentiometer 3           |
|   29 | A3 / POT4         | ADC            | Input     | Potentiometer 4           |

## Functional Groups

### Buttons (INPUT_PULLUP, active LOW, 50 ms debounce)

```
GPIO 2 ── BTN 1
GPIO 3 ── BTN 2
GPIO 4 ── BTN 3
GPIO 5 ── BTN 4
GPIO 6 ── BTN 5
```

### Bank Selector (INPUT_PULLUP, active LOW)

```
GPIO 7 ── BIT 0 ─┐
GPIO 8 ── BIT 1 ─┴─ 2-bit bank select (0–3)
```

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
GPIO 10 ── SDA1
GPIO 11 ── SCL1
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

| GPIO | Pico Pin | Capabilities          |
|-----:|---------:|-----------------------|
|    1 |        2 | UART0 RX, SPI0 CS, I2C0 SDA, PWM |
|    9 |       12 | SPI1 CS, I2C0 SCL, PWM |
|   12 |       16 | SPI1 RX, I2C0 SDA, PWM |
|   13 |       17 | SPI1 CS, I2C0 SCL, PWM |
|   14 |       19 | SPI1 SCK, I2C1 SDA, PWM |
|   15 |       20 | SPI1 TX, I2C1 SCL, PWM |
|   17 |       22 | SPI0 CS, I2C0 SCL, PWM |
|   18 |       24 | SPI0 SCK, I2C1 SDA, PWM |
|   19 |       25 | SPI0 TX, I2C1 SCL, PWM |
|   20 |       26 | SPI0 RX, I2C0 SDA, PWM |
|   21 |       27 | SPI0 CS, I2C0 SCL, PWM |
|   22 |       29 | SPI0 SCK, I2C1 SDA, PWM |

> **Note:** `config_eeprom.h` reserves EEPROM layout for 4 encoders (`MAX_ENCODERS = 4`) but no encoder pins are assigned yet.
