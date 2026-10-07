/*
 * Teletype MIDI Controller - EEPROM Configuration Map
 * RP2040 Emulated EEPROM (Flash Storage)
 *
 * Note: Emulated EEPROM uses flash memory with limited write endurance.
 * Configuration is committed only on an explicit Save command.
 */

#ifndef CONFIG_EEPROM_H
#define CONFIG_EEPROM_H

#include <stdint.h>

// ============================================================================
// MEMORY MAP OVERVIEW
// ============================================================================
// 0x0000-0x000F : Header (16 bytes) — flags at 0x0005, btn/pot MIDI channels at 0x0009/0x000A
// 0x0010-0x0023 : Button Mappings (20 bytes) - 5 buttons × 4 banks
// 0x0024-0x0027 : Reserved (padding)
// 0x0028-0x002F : Pot Mappings (8 bytes) - 4 pots × 2 bytes each
// 0x0030-0x005F : Reserved
// 0x0060-0x0061 : Checksum (2 bytes) — covers 0x0000-0x005F
// 0x0062-0x00FF : Reserved
// 0x0100-0x01FF : User Data (free space within the 512-byte allocation)

// ============================================================================
// HEADER SECTION (0x0000-0x000F)
// ============================================================================
#define EEPROM_ADDR_MAGIC    0x0000  // 4 bytes: "TTMC"
#define EEPROM_ADDR_VERSION  0x0004  // 1 byte: Version number
#define EEPROM_ADDR_FLAGS    0x0005  // 1 byte: Configuration flags (bits 0-3: ignore pots 1-4)
#define EEPROM_ADDR_NUM_BTNS 0x0006  // 1 byte: Total buttons
#define EEPROM_ADDR_NUM_POTS 0x0007  // 1 byte: Total pots
// 0x0008: Reserved (was encoder count)
#define EEPROM_ADDR_BTN_CH   0x0009  // 1 byte: Button MIDI channel (1-16)
#define EEPROM_ADDR_POT_CH   0x000A  // 1 byte: Pot MIDI channel (1-16)
// 0x000B-0x000F: Reserved

#define CONFIG_MAGIC_0 'T'
#define CONFIG_MAGIC_1 'T'
#define CONFIG_MAGIC_2 'M'
#define CONFIG_MAGIC_3 'C'
#define CONFIG_VERSION 0x07

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
// CHECKSUM (0x0060-0x0061)
// ============================================================================
// 16-bit sum of all bytes from 0x0000 to 0x005F (covers all config data)
#define EEPROM_ADDR_CHECKSUM 0x0060

// ============================================================================
// USER DATA (0x0100-0x01FF)
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

#define DEFAULT_BTN_MIDI_CHANNEL 1
#define DEFAULT_POT_MIDI_CHANNEL 1

#endif // CONFIG_EEPROM_H
