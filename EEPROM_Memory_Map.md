# OS MIDI Controller - EEPROM Memory Map

**Device:** RP2040 Emulated EEPROM (Flash Storage)  
**Size:** 512 bytes (configurable up to 4KB)  
**Version:** 1.1

> **Note:** The RP2040 does not have real EEPROM. This uses a 4KB flash sector 
> at the end of flash memory. Flash has limited write cycles (~100K), so avoid
> frequent writes to prevent premature wear.

---

## Memory Layout Overview

| Address Range   | Description            | Size       |
| --------------- | ---------------------- | ---------- |
| `0x0000-0x000F` | Header                 | 16 bytes   |
| `0x0010-0x001F` | Button Mappings        | 16 bytes   |
| `0x0020-0x002F` | Potentiometer Mappings | 16 bytes   |
| `0x0030-0x003F` | Encoder Mappings       | 16 bytes   |
| `0x0040-0x0041` | Checksum               | 2 bytes    |
| `0x0042-0x00FF` | Reserved               | 190 bytes  |
| `0x0100-0x07FF` | User Data (Free)       | 1792 bytes |

**Total Configuration Size:** 66 bytes  
**Free Space:** 1982 bytes

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
│ 0x0004  │ Version                              │ 0x01     │
│ 0x0005  │ Flags (reserved)                     │ 0x00     │
│ 0x0006  │ Total Buttons                        │ 16       │
│ 0x0007  │ Total Potentiometers                 │ 4        │
│ 0x0008  │ Total Encoders                       │ 4        │
│ 0x0009  │ Reserved                             │ -        │
│   ...   │ Reserved                             │ -        │
│ 0x000F  │ Reserved                             │ -        │
└─────────┴──────────────────────────────────────┴──────────┘
```

**Purpose:** Validates configuration integrity and version compatibility

---

### BUTTON MAPPINGS (0x0010-0x001F) - 16 bytes

```
┌─────────┬────────────┬─────────────────────────────┐
│ Address │ Button #   │ Description                 │
├─────────┼────────────┼─────────────────────────────┤
│ 0x0010  │ Button 1   │ MIDI Note (0-127)           │
│ 0x0011  │ Button 2   │ MIDI Note (0-127)           │
│ 0x0012  │ Button 3   │ MIDI Note (0-127)           │
│ 0x0013  │ Button 4   │ MIDI Note (0-127)           │
│ 0x0014  │ Button 5   │ MIDI Note (0-127)           │
│ 0x0015  │ Button 6   │ MIDI Note (0-127)           │
│ 0x0016  │ Button 7   │ MIDI Note (0-127)           │
│ 0x0017  │ Button 8   │ MIDI Note (future)          │
│ 0x0018  │ Button 9   │ MIDI Note (future)          │
│ 0x0019  │ Button 10  │ MIDI Note (future)          │
│ 0x001A  │ Button 11  │ MIDI Note (future)          │
│ 0x001B  │ Button 12  │ MIDI Note (future)          │
│ 0x001C  │ Button 13  │ MIDI Note (future)          │
│ 0x001D  │ Button 14  │ MIDI Note (future)          │
│ 0x001E  │ Button 15  │ MIDI Note (future)          │
│ 0x001F  │ Button 16  │ MIDI Note (future)          │
└─────────┴────────────┴─────────────────────────────┘
```

**Storage:** 1 byte per button (MIDI note number)  
**Currently Used:** Buttons 1-7 (physical)  
**Reserved:** Buttons 8-16 (expansion)

---

### POTENTIOMETER MAPPINGS (0x0020-0x002F) - 16 bytes

```
┌─────────┬──────┬────────────────────────────────────┐
│ Address │ Pot  │ Description                        │
├─────────┼──────┼────────────────────────────────────┤
│ 0x0020  │ Pot 1│ Type (0=CC, 1=PitchBend)           │
│ 0x0021  │ Pot 1│ Value (CC# if type=0, unused if =1)│
│ 0x0022  │ Pot 2│ Type (0=CC, 1=PitchBend)           │
│ 0x0023  │ Pot 2│ Value (CC# if type=0, unused if =1)│
│ 0x0024  │ Pot 3│ Type (0=CC, 1=PitchBend)           │
│ 0x0025  │ Pot 3│ Value (CC# if type=0, unused if =1)│
│ 0x0026  │ Pot 4│ Type (0=CC, 1=PitchBend)           │
│ 0x0027  │ Pot 4│ Value (CC# if type=0, unused if =1)│
│ 0x0028  │  -   │ Reserved                           │
│   ...   │  -   │ Reserved                           │
│ 0x002F  │  -   │ Reserved                           │
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

**Status:** Not yet implemented (future feature)

---

### CHECKSUM (0x0040-0x0041) - 2 bytes

```
┌─────────┬────────────────────────────────────────┐
│ Address │ Description                            │
├─────────┼────────────────────────────────────────┤
│ 0x0040  │ Checksum MSB (high byte)               │
│ 0x0041  │ Checksum LSB (low byte)                │
└─────────┴────────────────────────────────────────┘
```

**Algorithm:** 16-bit sum of all bytes from `0x0000` to `0x003F`  
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

### Buttons (Drum Kit Layout)
| Button | GPIO | Default Note | Instrument       |
| ------ | ---- | ------------ | ---------------- |
| 1      | 2    | 36           | Kick Drum        |
| 2      | 3    | 38           | Snare            |
| 3      | 4    | 42           | Closed Hi-Hat    |
| 4      | 5    | 46           | Open Hi-Hat      |
| 5      | 6    | 37           | Side Stick       |
| 6      | 7    | 39           | Clap             |
| 7      | 8    | 49           | Crash Cymbal     |
| 8-16   | -    | 48-56        | Future expansion |

### Potentiometers
| Pot | Analog Pin | Default Type | Default CC | Description       |
| --- | ---------- | ------------ | ---------- | ----------------- |
| 1   | A0         | CC           | 1          | Modulation Wheel  |
| 2   | A1         | CC           | 2          | Breath Controller |
| 3   | A2         | CC           | 74         | Cutoff/Brightness |
| 4   | A3         | CC           | 71         | Resonance/Timbre  |

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
/b <1-16> <0-127>       Map button to MIDI note
/p <1-4> cc <0-127>     Map pot to CC number
/p <1-4> pb             Map pot to Pitch Bend
/m                      Show all current mappings
```

### Storage Commands
```
/save                   Save configuration to EEPROM
/load                   Load configuration from EEPROM
/reset                  Reset to factory defaults
```

### Examples
```
/b 1 60                 Map button 1 to middle C
/p 2 pb                 Map pot 2 to pitch bend
/p 3 cc 7               Map pot 3 to volume (CC7)
/save                   Persist changes to EEPROM
```

---

## Boot Behavior

1. **Power On**
2. **Auto-Load:** Attempt to load configuration from EEPROM
3. **Validation:** Check magic bytes "OSMC" and checksum
4. **On Success:** Load saved configuration into RAM
5. **On Failure:** Use factory defaults (EEPROM not initialized or corrupted)

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

### Version 0x01 (Current)
- Switched to RP2040 emulated EEPROM (flash)
- Removed external I2C EEPROM (24LC16) dependency
- 16 button mappings (7 physical)
- 4 pot mappings (CC or Pitch Bend)
- 4 encoder mappings (reserved)
- Checksum validation
- Auto-load on boot

---

## Notes for Future Development

1. **Preset System:** Use address 0x0100+ for multiple configuration banks
2. **Name Storage:** Store UTF-8 strings for button/pot labels
3. **Calibration:** Store ADC min/max values for pots
4. **MIDI Channel:** Currently hardcoded to channel 1, could be configurable
5. **Velocity Curves:** Store button velocity response curves
6. **Encoder Implementation:** Physical encoders not yet implemented

---

**Document Version:** 1.0  
**Last Updated:** 2025-11-30  
**Maintained by:** OS MIDI Controller Project




