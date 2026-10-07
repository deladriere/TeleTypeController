# Teletype configuration storage

The RP2040 firmware allocates **512 bytes** of flash-backed emulated EEPROM. The configuration format is **0x07**, identified by the magic bytes `TTMC`.

The source of truth is [`src/config_eeprom.h`](src/config_eeprom.h), with serialization and validation in [`src/main.cpp`](src/main.cpp).

## Memory layout

| Address range | Purpose | Size |
| --- | --- | ---: |
| `0x0000–0x000F` | Header | 16 bytes |
| `0x0010–0x0023` | Button note mappings | 20 bytes |
| `0x0024–0x0027` | Reserved padding | 4 bytes |
| `0x0028–0x002F` | Pot mappings | 8 bytes |
| `0x0030–0x005F` | Reserved | 48 bytes |
| `0x0060–0x0061` | Checksum | 2 bytes |
| `0x0062–0x00FF` | Reserved | 158 bytes |
| `0x0100–0x01FF` | Unused user-data area | 256 bytes |

The configuration occupies the first **98 bytes**, including reserved bytes and checksum. The remaining **414 bytes** are unused by the current firmware. Addresses at or above `0x0200` are outside its 512-byte allocation.

## Header

| Address | Field | Value |
| --- | --- | --- |
| `0x0000–0x0003` | Magic | ASCII `TTMC` |
| `0x0004` | Format version | `0x07` |
| `0x0005` | Pot flags | Bits 0–3 mute pots 1–4; higher bits unused |
| `0x0006` | Number of note slots | 20 |
| `0x0007` | Number of pots | 4 |
| `0x0008` | Reserved | Written as zero |
| `0x0009` | Button MIDI channel | 1–16 |
| `0x000A` | Pot MIDI channel | 1–16 |
| `0x000B–0x000F` | Reserved | Unused |

## Mappings

Button mappings store one MIDI note number (0–127) per slot, ordered by bank:

| Slots | Bank | Physical buttons | Addresses | Factory notes |
| --- | --- | --- | --- | --- |
| 1–5 | 0 | 1–5 | `0x0010–0x0014` | 36–40 |
| 6–10 | 1 | 1–5 | `0x0015–0x0019` | 41–45 |
| 11–15 | 2 | 1–5 | `0x001A–0x001E` | 46–50 |
| 16–20 | 3 | 1–5 | `0x001F–0x0023` | 51–55 |

Each pot occupies two bytes starting at `0x0028 + 2 × (pot − 1)`:

1. Type: `0` for Control Change, `1` for Pitch Bend.
2. Value: CC number (0–127), unused when the type is Pitch Bend.

Factory pot mappings are CC **1, 2, 74, 71**, with all pots active. Both MIDI channels default to **1**.

## Checksum

The checksum is a 16-bit sum of all bytes from `0x0000` through `0x005F`, including reserved bytes. Its high byte is stored at `0x0060`, followed by the low byte at `0x0061`.

## Loading and saving

On startup, the firmware validates the magic, format version and checksum. A valid configuration is loaded into RAM; otherwise it uses factory defaults. Loading clamps invalid MIDI channels to their defaults and masks the pot flags to their four supported bits.

Changes made through the GUI or [USB MIDI SysEx](SYSEX_Protocol.md) update RAM first:

- **Save** writes the configuration and commits it to flash.
- **Load** restores saved settings; it returns an error if validation fails.
- **Reset** restores factory defaults in RAM. Save explicitly to persist them.

Flash has limited write endurance. The firmware commits only when asked to save. There is no serial text-command configuration interface in this firmware.
