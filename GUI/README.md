# OS MIDI Controller - Web Interface

Beautiful dark-themed web interface for configuring your OS MIDI Controller via Web Serial API.

## Features

✨ **Modern Dark Theme**
- Gradient backgrounds
- Smooth animations
- Responsive design

🔌 **Web Serial Connection**
- Direct serial communication from browser
- Real-time console output
- Send commands to device

🎛️ **Control Mappings**
- Configure 7 buttons (MIDI notes 0-127)
- Configure 4 potentiometers (CC or Pitch Bend)
- Instant apply with "Set" buttons
- Visual feedback

💾 **Configuration Management**
- Save to EEPROM
- Load from EEPROM
- Reset to factory defaults
- Load current mapping view

📟 **Live Console**
- See all device responses
- Color-coded messages
- Timestamps
- Auto-scroll

## Requirements

- **Chrome** or **Edge** browser (Web Serial API support)
- **OS MIDI Controller** connected via USB

## How to Use

### 1. Open the Interface

Simply open `controller.html` in Chrome or Edge:
```bash
# macOS
open controller.html

# Or drag the file into Chrome
```

### 2. Connect to Device

1. Click **"Connect Serial"**
2. Select your device from the popup (look for "usbmodem" on macOS)
3. Status will show "Connected" in green

### 3. Configure Mappings

#### Buttons
- Enter MIDI note (0-127) for each button
- Click **"Set"** to apply
- Notes are sent to device but not saved yet

#### Potentiometers
- Choose **"CC"** for Control Change or **"PB"** for Pitch Bend
- If CC: enter CC number (0-127)
- Click **"Set"** to apply

### 4. Save Configuration

After making changes:
1. Click **"💾 Save to EEPROM"** to apply the selected MIDI channel and persist changes
2. Device will write to EEPROM and confirm

### 5. Load/Reset

- **📥 Load from EEPROM**: Reload saved config from device
- **🔄 Reset to Defaults**: Restore factory settings
- **Show Current Mapping (/m)**: View current mappings in console

## Keyboard Shortcuts

None yet, but coming soon!

## Console Commands

The interface sends these commands to the device:

| Command | Description |
|---------|-------------|
| `/b <1-7> <0-127>` | Map button to MIDI note |
| `/p <1-4> cc <0-127>` | Map pot to CC |
| `/p <1-4> pb` | Map pot to Pitch Bend |
| `/pi <1-4> <0|1>` | Ignore pot ADC input |
| `/save` | Save to EEPROM |
| `/load` | Load from EEPROM |
| `/reset` | Reset to defaults |
| `/m` | Show current mappings |
| `/v` | Show firmware version |

## Default Mappings

### Buttons (Drum Kit)
| Button | Default Note | Instrument |
|--------|--------------|------------|
| 1 | 36 | Kick Drum |
| 2 | 38 | Snare |
| 3 | 42 | Closed Hi-Hat |
| 4 | 46 | Open Hi-Hat |
| 5 | 37 | Side Stick |
| 6 | 39 | Clap |
| 7 | 49 | Crash Cymbal |

### Potentiometers
| Pot | Default | Description |
|-----|---------|-------------|
| 1 | CC 1 | Modulation Wheel |
| 2 | CC 2 | Breath Controller |
| 3 | CC 74 | Cutoff/Brightness |
| 4 | CC 71 | Resonance/Timbre |

## Troubleshooting

### "Web Serial API not supported"
- **Solution**: Use Chrome (v89+) or Edge (v89+)
- Firefox and Safari don't support Web Serial yet

### Can't see device in port list
- **Solution**: 
  - Make sure device is plugged in
  - Check device shows up in system (macOS: `/dev/cu.usbmodem*`)
  - Try unplugging and replugging
  - Close other apps using the serial port

### Connected but no response
- **Solution**:
  - Check baud rate is 115200 (automatic in this interface)
  - Try disconnecting and reconnecting
  - Reset the device

### Changes not persisting after reboot
- **Solution**: Remember to click **"💾 Save to EEPROM"** after making changes!

## Advanced Usage

### Send Custom Commands

Open browser console (F12) and use:
```javascript
await sendCommand('/your_command');
```

### Monitor Serial Data

All received data is logged to the console area with timestamps.

## Browser Console (Developer Tools)

Press F12 to open developer console for debugging:
- Network errors
- Serial port issues
- JavaScript errors

## Security Notes

- **HTTPS Required**: Web Serial API requires HTTPS in production
- **User Permission**: Browser will always ask permission to access serial port
- **Local Files**: Works with `file://` protocol for local development

## Future Enhancements

- [ ] Preset management (save/load multiple configs)
- [ ] MIDI monitor (see notes in real-time)
- [ ] Export/import config as JSON
- [ ] Keyboard shortcuts
- [ ] Button test mode (highlight on press)
- [ ] Potentiometer visualization
- [ ] Dark/light theme toggle

## Credits

**OS MIDI Controller Interface v1.0**  
Built with vanilla HTML/CSS/JavaScript  
Uses Web Serial API  
Dark theme with gradient design

---

**Need Help?** Check the main project documentation or open an issue.

