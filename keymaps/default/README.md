# OffReno Custom TIDBIT Firmware

**A fully customized QMK firmware for the TIDBIT numpad with advanced system monitoring, volume balancing, Discord voice control, and smart home lighting.**

![TIDBIT Keyboard](https://i.imgur.com/placeholder.jpg) <!-- Replace with your actual photo -->

## 🎮 Features

### 🔄 Four Powerful Encoders

#### **Encoder 0 (Top Row)** - App Launcher & System Monitor
- **CW Rotation**: Cycle through apps to launch (Steam, Discord, Desktop WP, NordVPN)
- **CCW Rotation**: Cycle through apps to kill
- **Button Press**: Toggle system monitoring mode
- **Hold for 2 seconds**: Auto-execute selected app

**System Monitor Display:**
- CPU Load (%)
- GPU Load (%)
- VRAM Usage (%)
- Network Ping (ms)

#### **Encoder 1 (Second Row)** - Volume Balance Control
- **CW Rotation**: Increase game volume, decrease Discord volume
- **CCW Rotation**: Increase Discord volume, decrease game volume
- **Button Press (KC_P4)**: Reset volumes to 50/50 balance
- **KC_P5 Toggle**: Start/stop volume balance script

#### **Encoder 2 (Third Row)** - Discord Voice Control 🎙️
- **CW Rotation**: Next user in voice channel
- **CCW Rotation**: Previous user in voice channel
- **Button Press (KC_P1)**: Mute/unmute selected user
- **KC_P2 Toggle**: Start/stop Discord bot

**Display Messages:**
- Startup: "DICTATOR MODE ON" (3 seconds)
- Shutdown: "DEMOCRACY RESTORED" (3 seconds)
- Mute: "[Username] MUTED" (3 seconds)
- Unmute: "[Username] UNMUTED" (3 seconds)

#### **Encoder 3 (Fourth Row)** - LIFX Smart Lamp Control 💡
- **CW Rotation**: Increase lamp brightness (~10% per step)
- **CCW Rotation**: Decrease lamp brightness (~10% per step)
- **Button Press (KC_P0)**: Toggle lamp ON/OFF
- **Switch Next to Encoder (TOGGLE_LIFX)**: Start/stop LIFX control script

**Features:**
- Auto-discovers LIFX lamp on local network
- Smooth brightness transitions (100ms for adjustments, 500ms for power)
- Brightness range: 10% to 100%
- Works with any LIFX WiFi-enabled bulb

### 📺 OLED Display (128x32 SSD1306)
- Custom nullbits logo (idle state)
- App logos (Steam, Discord, Desktop WP, NordVPN)
- Real-time system stats
- Discord user list with index ([1/5])
- Dynamic message display

## 🛠️ Hardware Setup

### Hardware Configuration
- **MCU**: Frood RP2040 (converted from Pro Micro pinout)
- **Display**: SSD1306 OLED 128x32 (I2C)
- **Layout**:
  - First column: 4 rotary encoders
  - Remaining positions: 15 switches/keys
- **RGB**: WS2812, 8 LEDs

### Pin Configuration
- Encoders configured in [keyboard.json](keyboard.json) lines 44-50
- OLED on I2C (auto-configured for RP2040)
- Matrix pins defined in [keyboard.json](keyboard.json)

## 📦 Installation

### Prerequisites

#### Windows Requirements
1. **QMK MSYS** - [Download here](https://msys.qmk.fm/)
2. **Python 3.8+** - [Download here](https://www.python.org/downloads/)
3. **Git** - [Download here](https://git-scm.com/download/win)

#### Required Python Packages
```bash
pip install psutil hidapi pynvml pycaw comtypes keyboard discord.py lifxlan
```

Or using the requirements file:
```bash
pip install -r keymaps/default/requirements.txt
```

### Step 1: Clone QMK Firmware
```bash
git clone https://github.com/qmk/qmk_firmware.git
cd qmk_firmware
```

### Step 2: Install Your Custom Keymap
Copy the entire `tidbit` folder to:
```
qmk_firmware/keyboards/nullbitsco/tidbit/
```

Replace the existing folder completely.

### Step 3: Set Up Python Virtual Environment
```bash
cd keyboards/nullbitsco/tidbit
python -m venv .venv
.venv\Scripts\activate
pip install -r keymaps/default/requirements.txt
```

### Step 4: Update File Paths
Edit the following files and update the absolute paths:

**All .bat files** (`keymaps/default/*.bat`):
- `start_steam.bat`
- `start_discord.bat`
- `start_wallpaper.bat`
- `start_nordvpn.bat`
- `start_monitor.bat`
- `start_volume.bat`
- `kill_*.bat` files

Change this line in each:
```batch
cd /d "C:\Users\Renobatio\qmk_firmware\keyboards\nullbitsco\tidbit" >nul 2>&1
```
To your actual path:
```batch
cd /d "C:\Users\YOUR_USERNAME\qmk_firmware\keyboards\nullbitsco\tidbit" >nul 2>&1
```

**In keymap.c** (line 359):
```c
send_string("C:\\Users\\YOUR_USERNAME\\qmk_firmware\\keyboards\\nullbitsco\\tidbit\\keymaps\\default\\");
```

### Step 5: Compile Firmware
```bash
# From QMK root directory
make nullbitsco/tidbit:default CONVERT_TO=promicro_rp2040
```

### Step 6: Flash Firmware
```bash
make nullbitsco/tidbit:default:flash CONVERT_TO=promicro_rp2040
```

Or flash manually:
1. Hold BOOTSEL button on RP2040
2. Plug in USB
3. Drag `.uf2` file to RPI-RP2 drive

## 🎯 Discord Voice Control Setup

### Step 1: Create Discord Bot

1. Go to [Discord Developer Portal](https://discord.com/developers/applications)
2. Click "New Application"
3. Name it (e.g., "Petit Dictator")
4. Go to "Bot" section
5. Click "Add Bot"
6. Copy the bot token (you'll need this)

### Step 2: Enable Privileged Intents

In the Bot section, enable these intents:
- ✅ **PRESENCE INTENT**
- ✅ **SERVER MEMBERS INTENT**
- ✅ **MESSAGE CONTENT INTENT**

**CRITICAL:** Without these enabled in the Discord Developer Portal, the bot cannot see users in voice channels!

### Step 3: Invite Bot to Your Server

1. Go to "OAuth2" → "URL Generator"
2. Select scopes:
   - ✅ **bot**
3. Scroll down to "Bot Permissions" (only appears after selecting "bot")
4. Select permissions:
   - ✅ **View Channels**
   - ✅ **Mute Members**
5. Copy the generated URL
6. Open URL in browser and invite bot to your server

### Step 4: Configure Bot Credentials

Create `keymaps/default/discord_config.txt`:
```
BOT_TOKEN=your_bot_token_here
GUILD_ID=your_server_id_here
```

**To get Server ID:**
1. Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode)
2. Right-click your server name
3. Click "Copy Server ID"

### Step 5: Test Discord Bot

Run manually to test:
```bash
.venv\Scripts\python.exe keymaps/default/discord_voice_control.py
```

Check console output for:
- ✅ "Logged in as..."
- ✅ "Intents enabled: guilds=True, voice_states=True, members=True"
- ✅ "Connected to server..."
- ✅ "Found X non-bot users in voice"

If you see "Found 0 non-bot users" but you're in a voice channel, check that all intents are enabled in Discord Developer Portal!

## 🎮 Usage Guide

### System Monitoring
1. Press Encoder 0 button to toggle monitoring
2. Wait 5 seconds for startup
3. View real-time stats on OLED:
   - CPU: 45%
   - GPU: 78%
   - MEM: 62%
   - MS : 23ms

### Volume Balancing
1. Press KC_P5 (on main numpad) to start volume balance script
2. Rotate Encoder 1:
   - CW: Game louder, Discord quieter
   - CCW: Discord louder, Game quieter
3. Press Encoder 1 button (KC_P4) to reset to 50/50

### Discord Voice Control
1. Press KC_P2 to start Discord bot
2. See "DICTATOR MODE ON" for 3 seconds
3. OLED shows:
   ```
   Discord Voice
   [1/5]
   Username
   ```
4. Rotate Encoder 2 to cycle through users
5. Press Encoder 2 inward to mute/unmute
6. See "[Username] MUTED" for 3 seconds
7. Press KC_P2 again to stop bot

### App Launcher
1. Rotate Encoder 0 CW to cycle apps:
   - Steam → Discord → Desktop WP → NordVPN
2. Wait 2 seconds for auto-launch
3. Or rotate CCW to cycle kill mode
4. Wait 2 seconds to kill selected app

### LIFX Smart Lamp Control
1. **First Time Setup**:
   - Ensure LIFX lamp is on the same WiFi network as your PC
   - Lamp must be powered on and connected to WiFi
2. **Start the Script**:
   - Press the switch next to Encoder 3 (TOGGLE_LIFX)
   - Script will auto-discover your LIFX lamp (takes ~2 seconds)
   - Console shows: "Found LIFX lamp: [Your Lamp Name]"
3. **Control the Lamp**:
   - Press Encoder 3 inward: Toggle ON/OFF
   - Rotate CW: Increase brightness
   - Rotate CCW: Decrease brightness
4. **Stop the Script**:
   - Press TOGGLE_LIFX again to stop

**Note**: The LIFX control script must be running for the encoder to control the lamp. Use the TOGGLE_LIFX switch to start/stop it.

## 📁 File Structure

```
keymaps/default/
├── keymap.c                      # Main firmware code
├── rules.mk                      # Build rules
├── config.h                      # Configuration overrides
├── stubs.c                       # Compatibility stubs for RP2040
├── README.md                     # This file
├── requirements.txt              # Python dependencies
│
├── system_monitor.py             # System stats (CPU, GPU, RAM, Ping)
├── volume_balance.py             # Discord/Game volume control
├── discord_voice_control.py      # Discord user muting
├── lifx_control.py               # LIFX smart lamp control
│
├── discord_config.txt            # Discord bot credentials (YOU CREATE THIS)
│
├── start_monitor.bat             # Start system monitor
├── kill_monitor.bat              # Stop system monitor
├── start_volume.bat              # Start volume balancer
├── kill_volume.bat               # Stop volume balancer
├── start_discord.bat             # Start Discord bot
├── kill_discord.bat              # Stop Discord bot
├── start_lifx.bat                # Start LIFX lamp control
├── kill_lifx.bat                 # Stop LIFX lamp control
├── start_steam.bat               # Launch Steam
├── start_nordvpn.bat             # Launch NordVPN
├── start_wallpaper.bat           # Launch Wallpaper Engine
└── kill_*.bat                    # Kill respective apps
```

## 🔧 Troubleshooting

### Discord Bot Issues

**Problem:** "Found 0 non-bot users in voice"
- **Solution:** Enable PRESENCE INTENT, SERVER MEMBERS INTENT, and MESSAGE CONTENT INTENT in Discord Developer Portal (not just in code!)

**Problem:** "ERROR: Could not find guild with ID"
- **Solution:** Check GUILD_ID in discord_config.txt. Make sure it's the numeric server ID.

**Problem:** OLED shows "No users" but users are in voice
- **Solution:** Wait 5 seconds after bot startup. Voice states take time to populate.

### Compilation Errors

**Problem:** `make: command not found`
- **Solution:** Use QMK MSYS, not regular Windows Command Prompt

**Problem:** `Permission denied` when flashing
- **Solution:** Hold BOOTSEL button, plug USB, wait for drive to appear

### Python Script Issues

**Problem:** Script won't start from bat files
- **Solution:** Check absolute paths in all .bat files match your installation directory

**Problem:** `ModuleNotFoundError`
- **Solution:** Activate venv first: `.venv\Scripts\activate`, then install: `pip install -r requirements.txt`

## 🎨 Customization

### Change Encoder Functions
Edit `encoder_update_user()` in [keymap.c](keymap.c) starting at line 464

### Add New Apps to Launcher
Edit arrays in [keymap.c](keymap.c) lines 60-75:
```c
static const char* start_apps[] = {"Steam", "Discord", "Desktop WP", "NordVPN"};
static const char* start_bat_files[] = {
    "start_steam.bat",
    "start_discord.bat",
    "start_wallpaper.bat",
    "start_nordvpn.bat"
};
```

### Modify OLED Display
Edit `oled_task_user()` in [keymap.c](keymap.c) starting at line 553

### Change Discord Messages
Edit messages in [keymap.c](keymap.c):
- Line 432: Startup message
- Line 441: Shutdown message
- Lines 291, 296: Mute status messages

## 🌟 Credits

- **Base Firmware:** [QMK Firmware](https://qmk.fm/)
- **Keyboard Design:** [nullbits TIDBIT](https://nullbits.co/tidbit/)
- **Custom Firmware:** OffReno
- **Hardware:** Frood RP2040 + SSD1306 OLED

## 📝 License

This project is licensed under **GNU General Public License v2.0** (GPL-2.0).

### What This Means

✅ **You CAN:**
- Use this firmware for personal or commercial purposes
- Modify and customize the code
- Distribute your modified versions
- Learn from and study the code

⚠️ **You MUST:**
- Keep the same GPL-2.0 license on any modifications
- Provide source code if you distribute the firmware
- Credit the original author (OffReno) and QMK Firmware
- Share your improvements with the community

### Why GPL v2?

This project builds upon [QMK Firmware](https://github.com/qmk/qmk_firmware), which is licensed under GPL v2. Under GPL's "copyleft" terms, all derivative works (like this custom firmware) must maintain the same license. This ensures the code remains open and benefits the community.

### Attribution

If you use or modify this firmware, please credit:
- **Original Custom Firmware:** [OffReno](https://github.com/OffReno)
- **Base Firmware:** [QMK Firmware](https://qmk.fm/)
- **Keyboard Hardware:** [nullbits TIDBIT](https://nullbits.co/tidbit/)

---

**Made with ❤️ by OffReno**

⭐ If you find this useful, give it a star on GitHub!

For issues or questions, [open an issue](https://github.com/OffReno/custom-qmk-tidbit-firmware/issues).
