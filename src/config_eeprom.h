/*
 * OS MIDI Controller - EEPROM Configuration Map
 * 24LC16 (2KB) Memory Layout
 *
 * This file documents the EEPROM memory organization for AI reference
 * and future development.
 */

#ifndef CONFIG_EEPROM_H
#define CONFIG_EEPROM_H

#include <stdint.h> // For uint8_t, uint16_t types

// ============================================================================
// MEMORY MAP OVERVIEW
// ============================================================================
// 0x0000-0x000F : Header (16 bytes)
// 0x0010-0x001F : Button Mappings (16 bytes) - 16 buttons
// 0x0020-0x002F : Pot Mappings (16 bytes) - 4 pots × 2 bytes each
// 0x0030-0x003F : Encoder Mappings (16 bytes) - 4 encoders × 2 bytes each
// 0x0040-0x0041 : Checksum (2 bytes)
// 0x0100+       : User Data (free space)

// ============================================================================
// HEADER SECTION (0x0000-0x000F)
// ============================================================================
#define EEPROM_ADDR_MAGIC 0x0000    // 4 bytes: "OSMC"
#define EEPROM_ADDR_VERSION 0x0004  // 1 byte: Version number
#define EEPROM_ADDR_FLAGS 0x0005    // 1 byte: Configuration flags
#define EEPROM_ADDR_NUM_BTNS 0x0006 // 1 byte: Total buttons
#define EEPROM_ADDR_NUM_POTS 0x0007 // 1 byte: Total pots
#define EEPROM_ADDR_NUM_ENCS 0x0008 // 1 byte: Total encoders
// 0x0009-0x000F: Reserved

#define CONFIG_MAGIC_0 'O'
#define CONFIG_MAGIC_1 'S'
#define CONFIG_MAGIC_2 'M'
#define CONFIG_MAGIC_3 'C'
#define CONFIG_VERSION 0x01

// ============================================================================
// BUTTON MAPPINGS (0x0010-0x001F)
// ============================================================================
// Each button: 1 byte = MIDI Note number (0-127)
#define EEPROM_ADDR_BUTTONS 0x0010
#define MAX_BUTTONS 16

// ============================================================================
// POT MAPPINGS (0x0020-0x002F)
// ============================================================================
// Each pot: 2 bytes
//   Byte 0: Type (0=CC, 1=PitchBend)
//   Byte 1: Value (CC number if Type=0, unused if Type=1)
#define EEPROM_ADDR_POTS 0x0020
#define MAX_POTS 4
#define POT_TYPE_CC 0
#define POT_TYPE_PITCHBEND 1

// ============================================================================
// ENCODER MAPPINGS (0x0030-0x003F)
// ============================================================================
// Each encoder: 2 bytes
//   Byte 0: CC Number (0-127)
//   Byte 1: Mode (0=Relative, 1=Absolute)
#define EEPROM_ADDR_ENCODERS 0x0030
#define MAX_ENCODERS 4
#define ENC_MODE_RELATIVE 0
#define ENC_MODE_ABSOLUTE 1

// ============================================================================
// CHECKSUM (0x0040-0x0041)
// ============================================================================
// 16-bit checksum of bytes 0x0000-0x003F
#define EEPROM_ADDR_CHECKSUM 0x0040

// ============================================================================
// USER DATA (0x0100+)
// ============================================================================
#define EEPROM_ADDR_USERDATA 0x0100

// ============================================================================
// DEFAULT FACTORY MAPPINGS
// ============================================================================

// Default button to MIDI note mappings (Drum kit layout)
const uint8_t DEFAULT_BUTTON_NOTES[MAX_BUTTONS] = {
    36, // Button 1:  Kick Drum
    38, // Button 2:  Snare
    42, // Button 3:  Closed Hi-Hat
    46, // Button 4:  Open Hi-Hat
    37, // Button 5:  Side Stick
    39, // Button 6:  Clap
    49, // Button 7:  Crash Cymbal
    48, // Button 8:  Hi Tom (future)
    47, // Button 9:  Mid Tom (future)
    45, // Button 10: Low Tom (future)
    51, // Button 11: Ride Cymbal (future)
    53, // Button 12: Ride Bell (future)
    55, // Button 13: Splash Cymbal (future)
    52, // Button 14: Chinese Cymbal (future)
    44, // Button 15: Pedal Hi-Hat (future)
    56  // Button 16: Cowbell (future)
};

// Default pot mappings (Standard CCs 1-4)
const uint8_t DEFAULT_POT_TYPES[MAX_POTS] = {
    POT_TYPE_CC, // Pot 1: CC
    POT_TYPE_CC, // Pot 2: CC
    POT_TYPE_CC, // Pot 3: CC
    POT_TYPE_CC  // Pot 4: CC
};

const uint8_t DEFAULT_POT_VALUES[MAX_POTS] = {
    1,  // Pot 1: CC 1 (Modulation Wheel)
    2,  // Pot 2: CC 2 (Breath Controller)
    74, // Pot 3: CC 74 (Cutoff/Brightness)
    71  // Pot 4: CC 71 (Resonance/Timbre)
};

// Default encoder mappings
const uint8_t DEFAULT_ENC_CCS[MAX_ENCODERS] = {
    14, // Encoder 1: CC 14
    15, // Encoder 2: CC 15
    16, // Encoder 3: CC 16
    17  // Encoder 4: CC 17
};

const uint8_t DEFAULT_ENC_MODES[MAX_ENCODERS] = {
    ENC_MODE_RELATIVE, // Encoder 1: Relative mode
    ENC_MODE_RELATIVE, // Encoder 2: Relative mode
    ENC_MODE_RELATIVE, // Encoder 3: Relative mode
    ENC_MODE_RELATIVE  // Encoder 4: Relative mode
};

#endif // CONFIG_EEPROM_H
