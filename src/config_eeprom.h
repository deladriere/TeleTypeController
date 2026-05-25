/*
 * OS MIDI Controller - EEPROM Configuration Map
 * RP2040 Emulated EEPROM (Flash Storage)
 *
 * Note: Emulated EEPROM uses flash memory, limited to ~100K write cycles.
 * Don't write too frequently to avoid wearing out the flash.
 */

#ifndef CONFIG_EEPROM_H
#define CONFIG_EEPROM_H

#include <stdint.h>

// ============================================================================
// MEMORY MAP OVERVIEW
// ============================================================================
// 0x0000-0x000F : Header (16 bytes) — flags at 0x0005, MIDI channel at 0x0009, hwMode at 0x000A
// 0x0010-0x0023 : Button Mappings (20 bytes) - 5 buttons × 4 banks
// 0x0024-0x0027 : Reserved (padding)
// 0x0028-0x002F : Pot Mappings (8 bytes) - 4 pots × 2 bytes each
// 0x0030-0x003F : Encoder Mappings (16 bytes) - 4 encoders × 2 bytes each
// 0x0040-0x0041 : Reserved
// 0x0042-0x0044 : Sonic2 Config (3 bytes)
// 0x0045-0x0054 : Keypad Note Mappings (16 bytes) - 16 keys × 1 byte
// 0x0055-0x0057 : ToF VL53L0X Config (3 bytes)
// 0x0058-0x005F : Reserved
// 0x0060-0x0061 : Checksum (2 bytes) — covers 0x0000-0x005F
// 0x0100+       : User Data (free space)

// ============================================================================
// HEADER SECTION (0x0000-0x000F)
// ============================================================================
#define EEPROM_ADDR_MAGIC    0x0000  // 4 bytes: "OSMC"
#define EEPROM_ADDR_VERSION  0x0004  // 1 byte: Version number
#define EEPROM_ADDR_FLAGS    0x0005  // 1 byte: Configuration flags (bits 0-3: ignore pots 1-4)
#define EEPROM_ADDR_NUM_BTNS 0x0006  // 1 byte: Total buttons
#define EEPROM_ADDR_NUM_POTS 0x0007  // 1 byte: Total pots
#define EEPROM_ADDR_NUM_ENCS 0x0008  // 1 byte: Total encoders
#define EEPROM_ADDR_CHANNEL  0x0009  // 1 byte: Global MIDI channel (1-16)
#define EEPROM_ADDR_MODE     0x000A  // 1 byte: Hardware mode (0=buttons, 1=keypad, 2=keypad access)
// 0x000B-0x000F: Reserved

#define CONFIG_MAGIC_0 'O'
#define CONFIG_MAGIC_1 'S'
#define CONFIG_MAGIC_2 'M'
#define CONFIG_MAGIC_3 'C'
#define CONFIG_VERSION 0x04

// ============================================================================
// BUTTON MAPPINGS (0x0010-0x0023)
// ============================================================================
// 5 physical buttons × 4 banks = 20 note slots, 1 byte each (MIDI note 0-127)
// Layout: [Bank0: btn1-5][Bank1: btn1-5][Bank2: btn1-5][Bank3: btn1-5]
#define EEPROM_ADDR_BUTTONS  0x0010
#define MAX_BUTTONS          20   // 5 buttons × 4 banks
#define NUM_NOTE_BUTTONS      5   // Physical note buttons (pins 2-6)
#define NUM_BANKS             4   // 4 banks selected by 2-bit binary (pins 7-8)

// ============================================================================
// POT MAPPINGS (0x0028-0x002F)
// ============================================================================
// Each pot: 2 bytes
//   Byte 0: Type (0=CC, 1=PitchBend)
//   Byte 1: Value (CC number if Type=0, unused if Type=1)
#define EEPROM_ADDR_POTS    0x0028
#define MAX_POTS            4
#define POT_TYPE_CC         0
#define POT_TYPE_PITCHBEND  1
#define POT_FLAG_IGNORE_1   0x01
#define POT_FLAG_IGNORE_2   0x02
#define POT_FLAG_IGNORE_3   0x04
#define POT_FLAG_IGNORE_4   0x08
#define POT_FLAGS_IGNORE_MASK 0x0F

// ============================================================================
// ENCODER MAPPINGS (0x0030-0x003F)
// ============================================================================
// Each encoder: 2 bytes
//   Byte 0: CC Number (0-127)
//   Byte 1: Mode (0=Relative, 1=Absolute)
#define EEPROM_ADDR_ENCODERS 0x0030
#define MAX_ENCODERS         4
#define ENC_MODE_RELATIVE    0
#define ENC_MODE_ABSOLUTE    1

// ============================================================================
// I2C DEVICE CONFIG (0x0042-0x0044)
// ============================================================================
// Sonic2 ultrasonic distance sensor (addr 0x57)
//   Byte 0: MIDI type (0=CC, 1=PitchBend)
//   Byte 1: MIDI value (CC number if type=0)
//   Byte 2: Flags (bit0=invert — 1=closer is higher)
#define EEPROM_ADDR_I2C_DEVICES 0x0042
#define EEPROM_ADDR_SONIC       0x0042
#define SONIC_I2C_ADDR          0x57
#define SONIC_TYPE_CC           0
#define SONIC_TYPE_PITCHBEND    1
#define SONIC_FLAG_INVERT       0x01

// ============================================================================
// KEYPAD NOTE MAPPINGS (0x0045-0x0054)
// ============================================================================
// 4×4 matrix keypad: rows on pins 6-9 (OUTPUT), cols on pins 2-5 (INPUT_PULLUP)
// Key index = row*4 + col (0-15), stored as 1-based slots 1-16
//
//         Col0(2) Col1(3) Col2(4) Col3(5)
// Row0(6)   [1]    [2]    [3]    [A]    <- Slots  1- 4
// Row1(7)   [4]    [5]    [6]    [B]    <- Slots  5- 8
// Row2(8)   [7]    [8]    [9]    [C]    <- Slots  9-12
// Row3(9)   [*]    [0]    [#]    [D]    <- Slots 13-16
#define EEPROM_ADDR_KEYPAD  0x0045
#define MAX_KEYPAD_KEYS     16
#define HW_MODE_BUTTONS     0   // 5 push buttons + bank selector (pins 2-8)
#define HW_MODE_KEYPAD      1   // 4×4 keypad matrix (rows 2-5, cols 6-9)
#define HW_MODE_KEYPAD_ACCESS 2 // Type MIDI note digits, press # to send

// Human-readable key labels indexed 0-15 (internal linkage — safe in headers)
const char KEYPAD_LABELS[MAX_KEYPAD_KEYS][3] = {
    "1", "2", "3", "A",
    "4", "5", "6", "B",
    "7", "8", "9", "C",
    "*", "0", "#", "D"
};

// ============================================================================
// ToF VL53L0X CONFIG (0x0055-0x0057)
// ============================================================================
// ST VL53L0X time-of-flight distance sensor (addr 0x29)
//   Byte 0: MIDI type (0=CC, 1=PitchBend)
//   Byte 1: MIDI value (CC number if type=0)
//   Byte 2: Flags (bit0=invert — 1=closer is higher)
#define EEPROM_ADDR_TOF     0x0055
#define TOF_I2C_ADDR        0x29
#define TOF_TYPE_CC         0
#define TOF_TYPE_PITCHBEND  1
#define TOF_FLAG_INVERT     0x01

// ============================================================================
// CHECKSUM (0x0060-0x0061)
// ============================================================================
// 16-bit sum of all bytes from 0x0000 to 0x005F (covers all config data)
#define EEPROM_ADDR_CHECKSUM 0x0060

// ============================================================================
// USER DATA (0x0100+)
// ============================================================================
#define EEPROM_ADDR_USERDATA 0x0100

// ============================================================================
// DEFAULT FACTORY MAPPINGS
// ============================================================================

// Default button to MIDI note mappings (5 buttons × 4 banks = 20 slots)
const uint8_t DEFAULT_BUTTON_NOTES[MAX_BUTTONS] = {
    36, 37, 38, 39, 40,   // Bank 0
    41, 42, 43, 44, 45,   // Bank 1
    46, 47, 48, 49, 50,   // Bank 2
    51, 52, 53, 54, 55    // Bank 3
};

// Default pot mappings
const uint8_t DEFAULT_POT_TYPES[MAX_POTS] = {
    POT_TYPE_CC, POT_TYPE_CC, POT_TYPE_CC, POT_TYPE_CC
};

const uint8_t DEFAULT_POT_VALUES[MAX_POTS] = {
    1,   // Pot 1: CC 1  (Modulation Wheel)
    2,   // Pot 2: CC 2  (Breath Controller)
    74,  // Pot 3: CC 74 (Cutoff/Brightness)
    71   // Pot 4: CC 71 (Resonance/Timbre)
};

#define DEFAULT_MIDI_CHANNEL 1
#define DEFAULT_HW_MODE      HW_MODE_BUTTONS

// Default Sonic2 config: CC 11 (Expression), inverted (closer = higher)
#define DEFAULT_SONIC_TYPE  SONIC_TYPE_CC
#define DEFAULT_SONIC_VALUE 11
#define DEFAULT_SONIC_FLAGS SONIC_FLAG_INVERT

// Default ToF VL53L0X config: CC 20, inverted (closer = higher)
#define DEFAULT_TOF_TYPE  TOF_TYPE_CC
#define DEFAULT_TOF_VALUE 20
#define DEFAULT_TOF_FLAGS TOF_FLAG_INVERT

// Default keypad note mappings (16 keys, sequential from 36)
const uint8_t DEFAULT_KEYPAD_NOTES[MAX_KEYPAD_KEYS] = {
    36, 37, 38, 39,   // Row 0: "1","2","3","A"
    40, 41, 42, 43,   // Row 1: "4","5","6","B"
    44, 45, 46, 47,   // Row 2: "7","8","9","C"
    48, 49, 50, 51    // Row 3: "*","0","#","D"
};

// Default encoder mappings (reserved for future I2C encoders)
const uint8_t DEFAULT_ENC_CCS[MAX_ENCODERS]   = { 14, 15, 16, 17 };
const uint8_t DEFAULT_ENC_MODES[MAX_ENCODERS] = {
    ENC_MODE_RELATIVE, ENC_MODE_RELATIVE,
    ENC_MODE_RELATIVE, ENC_MODE_RELATIVE
};

#endif // CONFIG_EEPROM_H
