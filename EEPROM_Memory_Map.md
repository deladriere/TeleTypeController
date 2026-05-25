# OS MIDI Controller - EEPROM Memory Map

**Device:** RP2040 Emulated EEPROM (Flash Storage)  
**Size:** 512 bytes (configurable up to 4KB)  
**Version:** 0x04

> **Note:** The RP2040 does not have real EEPROM. This uses a 4KB flash sector 
> at the end of flash memory. Flash has limited write cycles (~100K), so avoid
> frequent writes to prevent premature wear.

---

## Memory Layout Overview

| Address Range   | Description            | Size       |
| --------------- | ---------------------- | ---------- |
| `0x0000-0x000F` | Header                 | 16 bytes   |
| `0x0010-0x0023` | Button Mappings        | 20 bytes   |
| `0x0024-0x0027` | Reserved (padding)     | 4 bytes    |
| `0x0028-0x002F` | Potentiometer Mappings | 8 bytes    |
| `0x0030-0x003F` | Encoder Mappings       | 16 bytes   |
| `0x0040-0x0041` | Reserved               | 2 bytes    |
| `0x0042-0x0044` | Sonic2 Config          | 3 bytes    |
| `0x0045-0x0054` | Keypad Note Mappings   | 16 bytes   |
| `0x0055-0x0057` | ToF VL53L0X Config     | 3 bytes    |
| `0x0058-0x005F` | Reserved               | 8 bytes    |
| `0x0060-0x0061` | Checksum               | 2 bytes    |
| `0x0062-0x00FF` | Reserved               | 158 bytes  |
| `0x0100-0x07FF` | User Data (Free)       | 1792 bytes |

**Total Configuration Size:** 101 bytes (0x0000–0x0061)  
**Free Space:** 1899 bytes

---

## Detailed Memory Map

### HEADER (0x0000-0x000F) - 16 bytes

```
┌─────────┬──────────────────────────────────────┬──────────┐
│ Address │ Description                          │ Value    │
├─────────┼──────────────────────────────────────┼──────────┤
│ 0x0000  │ Magic Byte 0                         │ 'O' (79) │
│ 0x0001  │ Magic Byte 1                         │ 'S' (83) │
│ 0x0002  │ Magic Byte 2                         │ 'M' (77) │
│ 0x0003  │ Magic Byte 3                         │ 'C' (67) │
│ 0x0004  │ Version                              │ 0x02     │
│ 0x0005  │ Flags (reserved)                     │ 0x00     │
│ 0x0006  │ Total Buttons                        │ 20       │
│ 0x0007  │ Total Potentiometers                 │ 4        │
│ 0x0008  │ Total Encoders                       │ 4        │
│ 0x0009  │ Global MIDI Channel                  │ 1-16     │
│ 0x000A  │ Hardware Mode (0=buttons, 1=keypad)  │ 0 or 1   │
│ 0x000B  │ Reserved                             │ -        │
│   ...   │ Reserved                             │ -        │
│ 0x000F  │ Reserved                             │ -        │
└─────────┴──────────────────────────────────────┴──────────┘
```

**Purpose:** Validates configuration integrity and version compatibility

---

### BUTTON MAPPINGS (0x0010-0x0023) - 20 bytes

```
┌─────────┬────────────┬─────────────────────────────┐
│ Address │ Button #   │ Description                 │
├─────────┼────────────┼─────────────────────────────┤
│ 0x0010  │ Button 1   │ MIDI Note (0-127)           │
│ 0x0011  │ Button 2   │ MIDI Note (0-127)           │
│ 0x0012  │ Button 3   │ MIDI Note (0-127)           │
│ 0x0013  │ Button 4   │ MIDI Note (0-127)           │
│ 0x0014  │ Button 5   │ MIDI Note (0-127)           │
│ 0x0015  │ Button 6   │ MIDI Note (Bank 1, btn 1)   │
│ 0x0016  │ Button 7   │ MIDI Note (Bank 1, btn 2)   │
│ 0x0017  │ Button 8   │ MIDI Note (Bank 1, btn 3)   │
│ 0x0018  │ Button 9   │ MIDI Note (Bank 1, btn 4)   │
│ 0x0019  │ Button 10  │ MIDI Note (Bank 1, btn 5)   │
│ 0x001A  │ Button 11  │ MIDI Note (Bank 2, btn 1)   │
│ 0x001B  │ Button 12  │ MIDI Note (Bank 2, btn 2)   │
│ 0x001C  │ Button 13  │ MIDI Note (Bank 2, btn 3)   │
│ 0x001D  │ Button 14  │ MIDI Note (Bank 2, btn 4)   │
│ 0x001E  │ Button 15  │ MIDI Note (Bank 2, btn 5)   │
│ 0x001F  │ Button 16  │ MIDI Note (Bank 3, btn 1)   │
│ 0x0020  │ Button 17  │ MIDI Note (Bank 3, btn 2)   │
│ 0x0021  │ Button 18  │ MIDI Note (Bank 3, btn 3)   │
│ 0x0022  │ Button 19  │ MIDI Note (Bank 3, btn 4)   │
│ 0x0023  │ Button 20  │ MIDI Note (Bank 3, btn 5)   │
└─────────┴────────────┴─────────────────────────────┘
```

**Storage:** 1 byte per button (MIDI note number)  
**Layout:** 5 buttons x 4 banks = 20 slots  
**Physical buttons:** 5 (GPIO 2-6), bank-selected by GPIO 7-8

---

### POTENTIOMETER MAPPINGS (0x0028-0x002F) - 8 bytes

```
┌─────────┬──────┬────────────────────────────────────┐
│ Address │ Pot  │ Description                        │
├─────────┼──────┼────────────────────────────────────┤
│ 0x0028  │ Pot 1│ Type (0=CC, 1=PitchBend)           │
│ 0x0029  │ Pot 1│ Value (CC# if type=0, unused if =1)│
│ 0x002A  │ Pot 2│ Type (0=CC, 1=PitchBend)           │
│ 0x002B  │ Pot 2│ Value (CC# if type=0, unused if =1)│
│ 0x002C  │ Pot 3│ Type (0=CC, 1=PitchBend)           │
│ 0x002D  │ Pot 3│ Value (CC# if type=0, unused if =1)│
│ 0x002E  │ Pot 4│ Type (0=CC, 1=PitchBend)           │
│ 0x002F  │ Pot 4│ Value (CC# if type=0, unused if =1)│
└─────────┴──────┴────────────────────────────────────┘
```

**Storage:** 2 bytes per pot
- **Byte 0:** Type flag (0=CC, 1=Pitch Bend)
- **Byte 1:** CC number (0-127) if Type=0

**Note:** Pitch Bend is a 14-bit MIDI message (not a CC)

---

### ENCODER MAPPINGS (0x0030-0x003F) - 16 bytes

```
┌─────────┬──────────┬────────────────────────────────┐
│ Address │ Encoder  │ Description                    │
├─────────┼──────────┼────────────────────────────────┤
│ 0x0030  │ Encoder 1│ CC Number (0-127)              │
│ 0x0031  │ Encoder 1│ Mode (0=Relative, 1=Absolute)  │
│ 0x0032  │ Encoder 2│ CC Number (0-127)              │
│ 0x0033  │ Encoder 2│ Mode (0=Relative, 1=Absolute)  │
│ 0x0034  │ Encoder 3│ CC Number (0-127)              │
│ 0x0035  │ Encoder 3│ Mode (0=Relative, 1=Absolute)  │
│ 0x0036  │ Encoder 4│ CC Number (0-127)              │
│ 0x0037  │ Encoder 4│ Mode (0=Relative, 1=Absolute)  │
│ 0x0038  │    -     │ Reserved                       │
│   ...   │    -     │ Reserved                       │
│ 0x003F  │    -     │ Reserved                       │
└─────────┴──────────┴────────────────────────────────┘
```

**Storage:** 2 bytes per encoder
- **Byte 0:** CC number
- **Byte 1:** Mode (0=Relative, 1=Absolute)

**Status:** Reserved for future I2C-based encoders

---

### SONIC2 CONFIG (0x0042-0x0044) - 3 bytes

```
┌─────────┬────────────────────────────────────────┐
│ Address │ Description                            │
├─────────┼────────────────────────────────────────┤
│ 0x0042  │ MIDI Type (0=CC, 1=PitchBend)          │
│ 0x0043  │ MIDI Value (CC# if type=0)             │
│ 0x0044  │ Flags (bit 0: invert, 1=closer=higher) │
└─────────┴────────────────────────────────────────┘
```

**I2C Address:** 0x57  
**Sensor:** M5Stack Unit Sonic2 ultrasonic distance sensor  
**Range used:** 20–600 mm (hand gesture range)  
**Auto-detect:** Probed on boot and via `/i` scan

---

### KEYPAD NOTE MAPPINGS (0x0045-0x0054) - 16 bytes

4×4 matrix keypad: rows on pins 2-5 (OUTPUT), cols on pins 6-9 (INPUT_PULLUP).  
Key index = row×4 + col (0-based). Stored as 1-based slots 1-16 in the commands.

```
┌─────────┬──────────┬────────────────────────────────┐
│ Address │ Slot     │ Description                    │
├─────────┼──────────┼────────────────────────────────┤
│ 0x0045  │ Slot  1  │ Key "1" (Row0/Col0) MIDI Note  │
│ 0x0046  │ Slot  2  │ Key "2" (Row0/Col1) MIDI Note  │
│ 0x0047  │ Slot  3  │ Key "3" (Row0/Col2) MIDI Note  │
│ 0x0048  │ Slot  4  │ Key "A" (Row0/Col3) MIDI Note  │
│ 0x0049  │ Slot  5  │ Key "4" (Row1/Col0) MIDI Note  │
│ 0x004A  │ Slot  6  │ Key "5" (Row1/Col1) MIDI Note  │
│ 0x004B  │ Slot  7  │ Key "6" (Row1/Col2) MIDI Note  │
│ 0x004C  │ Slot  8  │ Key "B" (Row1/Col3) MIDI Note  │
│ 0x004D  │ Slot  9  │ Key "7" (Row2/Col0) MIDI Note  │
│ 0x004E  │ Slot 10  │ Key "8" (Row2/Col1) MIDI Note  │
│ 0x004F  │ Slot 11  │ Key "9" (Row2/Col2) MIDI Note  │
│ 0x0050  │ Slot 12  │ Key "C" (Row2/Col3) MIDI Note  │
│ 0x0051  │ Slot 13  │ Key "*" (Row3/Col0) MIDI Note  │
│ 0x0052  │ Slot 14  │ Key "0" (Row3/Col1) MIDI Note  │
│ 0x0053  │ Slot 15  │ Key "#" (Row3/Col2) MIDI Note  │
│ 0x0054  │ Slot 16  │ Key "D" (Row3/Col3) MIDI Note  │
└─────────┴──────────┴────────────────────────────────┘
```

**Command:** `/k <1-16> <0-127>` — map key slot to MIDI note

---

### CHECKSUM (0x0060-0x0061) - 2 bytes

```
┌─────────┬────────────────────────────────────────┐
│ Address │ Description                            │
├─────────┼────────────────────────────────────────┤
│ 0x0060  │ Checksum MSB (high byte)               │
│ 0x0061  │ Checksum LSB (low byte)                │
└─────────┴────────────────────────────────────────┘
```

**Algorithm:** 16-bit sum of all bytes from `0x0000` to `0x005F`  
**Purpose:** Detects EEPROM corruption

---

### USER DATA (0x0100-0x07FF) - 1792 bytes

**Status:** Reserved for future use  
**Potential Uses:**
- Preset banks (multiple configurations)
- Button/pot names (strings)
- Calibration data
- Custom MIDI sequences
- User notes/documentation

---

## Factory Default Values

### Global Settings
| Setting      | Default |
| ------------ | ------- |
| MIDI Channel | 1       |

### Buttons (Sequential Layout)
| Slot  | Bank | Button | Default Note |
| ----- | ---- | ------ | ------------ |
| 1-5   | 0    | 1-5    | 36-40        |
| 6-10  | 1    | 1-5    | 41-45        |
| 11-15 | 2    | 1-5    | 46-50        |
| 16-20 | 3    | 1-5    | 51-55        |

### Potentiometers
| Pot | Analog Pin | Default Type | Default CC | Description       |
| --- | ---------- | ------------ | ---------- | ----------------- |
| 1   | A0         | CC           | 1          | Modulation Wheel  |
| 2   | A1         | CC           | 2          | Breath Controller |
| 3   | A2         | CC           | 74         | Cutoff/Brightness |
| 4   | A3         | CC           | 71         | Resonance/Timbre  |

### Sonic2 (I2C Distance Sensor)
| Setting    | Default    | Description                  |
| ---------- | ---------- | ---------------------------- |
| MIDI Type  | CC         | Control Change               |
| MIDI Value | 11         | CC 11 (Expression)           |
| Flags      | 0x01       | Inverted (closer = higher)   |

### Encoders (Future)
| Encoder | Default CC | Default Mode |
| ------- | ---------- | ------------ |
| 1       | 14         | Relative     |
| 2       | 15         | Relative     |
| 3       | 16         | Relative     |
| 4       | 17         | Relative     |

---

## Configuration Commands

### Mapping Commands
```
/b <1-20> <0-127>       Map button slot to MIDI note
/p <1-4> cc <0-127>     Map pot to CC number
/p <1-4> pb             Map pot to Pitch Bend
/pi <1-4> <0|1>         Ignore pot ADC input (1=ignore, 0=active)
/ch <1-16>              Set global MIDI channel
/s cc <0-127>           Map Sonic2 to CC number
/s pb                   Map Sonic2 to Pitch Bend
/m                      Show all current mappings
```

### Storage Commands
```
/save                   Save configuration to EEPROM
/load                   Load configuration from EEPROM
/reset                  Reset to factory defaults
```

### Info Commands
```
/h                      Show command help
/v                      Show firmware version
/d                      Dump EEPROM contents
/test                   Test button/bank states
/a                      Toggle ADC debug (raw pot values)
/i                      Scan I2C bus (with device identification)
```

### Examples
```
/b 1 60                 Map button 1 to middle C
/p 2 pb                 Map pot 2 to pitch bend
/p 3 cc 7               Map pot 3 to volume (CC7)
/ch 10                  Set MIDI channel to 10 (drums)
/s cc 74                Map Sonic2 to cutoff (CC74)
/save                   Persist changes to EEPROM
```

---

## Boot Behavior

1. **Power On**
2. **I2C Probe:** Check for Sonic2 at address 0x57
3. **Auto-Load:** Attempt to load configuration from EEPROM
4. **Validation:** Check magic bytes "OSMC", version 0x02, and checksum
5. **On Success:** Load saved configuration into RAM
6. **On Failure:** Use factory defaults (EEPROM not initialized or corrupted)

**Note:** All changes are made in RAM first. Must use `/save` to persist to EEPROM.

---

## Technical Details

### RP2040 Emulated EEPROM
- **Storage:** 4KB flash sector at end of flash memory
- **Configured Size:** 512 bytes (adjustable via `EEPROM.begin(size)`)
- **Max Size:** 4096 bytes
- **Write Endurance:** ~100,000 cycles (flash limitation)
- **Library:** `<EEPROM.h>` (earlephilhower Arduino-Pico core)

### API Usage
```cpp
EEPROM.begin(512);           // Initialize with size
EEPROM.read(addr);           // Read byte at address
EEPROM.write(addr, data);    // Write byte (buffered in RAM)
EEPROM.commit();             // Write RAM buffer to flash
EEPROM.end();                // Commit and free memory
```

### Important Notes
- Writes are buffered in RAM until `EEPROM.commit()` is called
- No external wiring required - all internal to RP2040
- Data persists across power cycles

---

## Version History

### Version 0x02 (Current)
- Added global MIDI channel (configurable 1-16)
- Added Sonic2 I2C distance sensor support (0x57)
- Added I2C device auto-detection on boot
- Extended checksum range to 0x0000-0x004F
- Moved checksum address from 0x0040 to 0x0050
- New serial commands: `/ch`, `/s`, enhanced `/i`

### Version 0x01
- Switched to RP2040 emulated EEPROM (flash)
- Removed external I2C EEPROM (24LC16) dependency
- 20 button mappings (5 physical x 4 banks)
- 4 pot mappings (CC or Pitch Bend)
- 4 encoder mappings (reserved)
- Checksum validation
- Auto-load on boot

---

## Notes for Future Development

1. **Preset System:** Use address 0x0100+ for multiple configuration banks
2. **Name Storage:** Store UTF-8 strings for button/pot labels
3. **Calibration:** Store ADC min/max values for pots
4. **Velocity Curves:** Store button velocity response curves
5. **I2C Encoders:** Use devices at 0x0045-0x004F for I2C encoder breakouts
6. **Additional I2C Sensors:** Extend 0x0045+ for new device types

---

**Document Version:** 2.0  
**Last Updated:** 2026-03-03  
**Maintained by:** OS MIDI Controller Project
