/*
 * Teletype MIDI Controller — SysEx protocol
 *
 * Frame: F0 7D 54 54 <cmd> [payload...] F7
 *   7D     = MIDI non-commercial / educational manufacturer ID
 *   54 54  = 'T''T' device identity
 *
 * All payload bytes are 0-127 (7-bit MIDI data).
 */

#ifndef SYSEX_PROTOCOL_H
#define SYSEX_PROTOCOL_H

#include <stdint.h>

#define SYSEX_MFG           0x7D
#define SYSEX_DEV0          0x54  // 'T'
#define SYSEX_DEV1          0x54  // 'T'

// Host → Device
#define SX_GET_VERSION      0x01
#define SX_GET_CONFIG       0x02
#define SX_SET_BTN_NOTE     0x10  // slot(1-20), note(0-127)
#define SX_SET_POT          0x11  // pot(1-4), type(0=CC/1=PB), value(0-127)
#define SX_SET_POT_IGNORE   0x12  // pot(1-4), ignore(0/1)
#define SX_SET_BTN_CH       0x13  // channel(1-16)
#define SX_SET_POT_CH       0x14  // channel(1-16)
#define SX_SAVE             0x20
#define SX_LOAD             0x21
#define SX_RESET            0x22

// Device → Host
#define SX_VERSION          0x41  // ASCII version string (no null)
#define SX_CONFIG           0x42  // see layout below
#define SX_ACK              0x4F  // cmd, status(0=ok, 1=err)

// SX_CONFIG payload layout (31 bytes):
//   [0]     btnChannel (1-16)
//   [1]     potChannel (1-16)
//   [2]     potFlags
//   [3..22] buttonNotes[20]
//   [23..26] potTypes[4]
//   [27..30] potValues[4]
#define SX_CONFIG_LEN       31

#define SX_STATUS_OK        0
#define SX_STATUS_ERR       1

#endif // SYSEX_PROTOCOL_H
