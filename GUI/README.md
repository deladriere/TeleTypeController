# Teletype Monitor & Configure

[`index.html`](index.html) combines the device drawing and MIDI configuration interface. [`controller.html`](controller.html) redirects here, as does the repository root `index.html` for GitHub Pages.

## Run

Open **[Teletype online](https://deladriere.github.io/TeleTypeController/)** in desktop Chrome or Edge.

Open `GUI/index.html` in desktop Chrome or Edge, keeping the CSS and JavaScript alongside it. If MIDI permissions are unavailable on a local file, serve the repository root:

```sh
python3 -m http.server 8000 --bind 127.0.0.1
```

Visit [http://localhost:8000/GUI/](http://localhost:8000/GUI/). Online hosting must use HTTPS. No npm dependencies or frontend build are required. The SVG is inline.

## Files

- `index.html`: page structure and the inline Teletype SVG.
- `style.css`: styling adapted from the original visual study.
- `appearance.js`: enclosure palettes and browser-local color preference; no MIDI access.
- `visual.js`: read-only key flashes, knob indicators and inferred bank display; no gesture handlers or outgoing messages.
- `protocol.js`: validated SysEx configuration frames and ordinary MIDI interpretation.
- `app.js`: MIDI ports, configuration handshake and queued configuration requests.

## Device communication

The app requests MIDI/SysEx access and pairs a unique input/output by name and manufacturer. It prefers Teletype and reads the firmware version and configuration before enabling settings changes. If identical device names are ambiguous, connect one controller at a time. Reconnection is explicit after USB loss.

The monitor consumes ordinary Note On, CC and pitch bend on the configured channels. It sends no MIDI, performs no polling, and needs no new firmware commands. Note Off and zero-velocity Note On do not trigger flashes. Values are unknown until received; reconnecting or applying configuration clears the display's previous observations.

A unique note mapping identifies its key and bank. Duplicate notes are shown as ambiguous without identifying a key or bank. Bank switches send no messages: the display shows the bank inferred from the last unique note, not a live reading of the switches. Knobs sharing a CC or pitch bend update together. Muted knobs are excluded from matching. Pitch bend is displayed as a signed MIDI value, with knob indicators normalized to 0–127.

Only connection setup (GET_VERSION / GET_CONFIG) and explicit Configure actions send SysEx. Set applies mapping, channel and mute edits. Save is blocked while edits are unapplied. Load and Reset refresh configuration after confirmation. Firmware v1.0.1 retains the original MIDI/configuration behavior and fixes USB initialization to preserve the serial upload interface. It has no remote Play commands or added feedback stream.

## Checks

Run `sh tests/run.sh` and `pio run -e pico` from the repository root. Host tests cover configuration framing, channel filtering, ambiguous mappings, physical performance output and mute behavior.

Browser validation uses simulated MIDI to check incoming updates, no output from monitoring or screen gestures, configuration and reconnecting, at desktop and narrow widths. This does not replace real-device validation.

Protocol: [`SYSEX_Protocol.md`](../SYSEX_Protocol.md). User guide: [`README.md`](../README.md).
