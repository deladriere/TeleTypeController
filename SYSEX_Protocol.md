# Teletype MIDI Controller — SysEx Protocol

Config runs over **USB MIDI SysEx** only. Hardware Serial MIDI (TX0) carries performance messages (notes, CC and pitch bend).

## Frame

```
F0  7D  54  54  <cmd>  [payload...]  F7
│   │   │   │
│   │   └──┴── 'T''T' device ID
│   └───────── manufacturer 0x7D (non-commercial / educational)
└───────────── SysEx start
```

All payload bytes are 7-bit (0–127).

## Host → Device

| Cmd  | Name            | Payload                                      |
|------|-----------------|----------------------------------------------|
| 0x01 | GET_VERSION     | —                                            |
| 0x02 | GET_CONFIG      | —                                            |
| 0x10 | SET_BTN_NOTE    | slot(1–20), note(0–127)                       |
| 0x11 | SET_POT         | pot(1–4), type(0=CC/1=PB), value(0–127)      |
| 0x12 | SET_POT_IGNORE  | pot(1–4), ignore(0/1)                        |
| 0x13 | SET_BTN_CH      | channel(1–16)                                |
| 0x14 | SET_POT_CH      | channel(1–16)                                |
| 0x20 | SAVE            | —                                            |
| 0x21 | LOAD            | —                                            |
| 0x22 | RESET           | —                                            |

## Device → Host

| Cmd  | Name     | Payload                                      |
|------|----------|----------------------------------------------|
| 0x41 | VERSION  | ASCII version (e.g. `v1.0.1`)                  |
| 0x42 | CONFIG   | 31 bytes, see layout below                   |
| 0x4F | ACK      | echoed_cmd, status(0=ok, 1=err)              |

### CONFIG layout (31 bytes)

| Offset | Size | Field            |
|--------|------|------------------|
| 0      | 1    | btnChannel       |
| 1      | 1    | potChannel       |
| 2      | 1    | potFlags         |
| 3–22   | 20   | buttonNotes[20]  |
| 23–26  | 4    | potTypes[4]      |
| 27–30  | 4    | potValues[4]     |

Bits 0–3 of `potFlags` mute pots 1–4 (`1` = muted). Button notes are ordered by bank: bank 0 buttons 1–5, then banks 1, 2 and 3. Pot types use `0` for CC and `1` for pitch bend; a pot value is its CC number and is unused for pitch bend. For `SET_POT` with pitch bend, send `0` as the unused value byte.

## Command behavior

- `GET_VERSION` returns `VERSION`; `GET_CONFIG` returns `CONFIG`.
- Mapping, mute and channel commands update RAM and return `ACK`.
- `SAVE` writes the working configuration to flash and returns `ACK`. The current implementation does not propagate the flash commit result, so this ACK is not a read-back verification.
- `LOAD` returns an OK `ACK` followed by `CONFIG` when saved data validates. Invalid saved data returns an error `ACK` and leaves the working configuration unchanged.
- `RESET` restores factory defaults in RAM, returns an OK `ACK`, then `CONFIG`. Send `SAVE` separately to persist defaults.
- Unknown commands and invalid mapping/channel parameters return an error `ACK`. Frames for a different device identity are ignored.

There is no serial text-command equivalent in this firmware.

Source of truth: [`src/sysex_protocol.h`](src/sysex_protocol.h)
