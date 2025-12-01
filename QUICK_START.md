# Quick Start Guide

## 🚀 One-Page Setup Summary

### What You Have
- **4 Encoders** with custom functions
- **OLED Display** showing stats and logos
- **3 Python Scripts** running in background
- **Discord Bot** for voice channel control

### Quick Reference Card

| Control | Function |
|---------|----------|
| **Encoder 0** | |
| Rotate CW/CCW | Cycle apps (launch/kill modes) |
| Press | Toggle system monitor |
| **Encoder 1** | |
| Rotate CW | Game louder, Discord quieter |
| Rotate CCW | Discord louder, Game quieter |
| Press (KC_P4) | Reset to 50/50 balance |
| KC_P5 | Start/stop volume script |
| **Encoder 2** | |
| Rotate CW/CCW | Cycle Discord users |
| Press (KC_P1) | Mute/unmute selected user |
| KC_P2 | Start/stop Discord bot |

### Files You Need to Update

Before compiling, update these paths from `C:\Users\Renobatio\...` to your actual path:

1. **All .bat files** (14 files in `keymaps/default/`)
2. **keymap.c** line 359

### Discord Bot Checklist

- [ ] Create bot at https://discord.com/developers/applications
- [ ] Enable PRESENCE INTENT
- [ ] Enable SERVER MEMBERS INTENT
- [ ] Enable MESSAGE CONTENT INTENT
- [ ] Copy bot token
- [ ] Get server ID (right-click server, Copy ID)
- [ ] Create `discord_config.txt` with token and server ID
- [ ] Invite bot to server with "View Channels" and "Mute Members" permissions

### Compile Commands

```bash
# Compile
make nullbitsco/tidbit:default CONVERT_TO=promicro_rp2040

# Flash
make nullbitsco/tidbit:default:flash CONVERT_TO=promicro_rp2040
```

### Testing Checklist

- [ ] System monitor shows stats when encoder 0 pressed
- [ ] Volume balance works with encoder 1
- [ ] Discord bot detects users in voice channels
- [ ] Encoder 2 cycles through Discord users
- [ ] Mute/unmute works correctly
- [ ] OLED displays messages clearly
- [ ] All startup/shutdown messages appear

### Common Issues

| Problem | Solution |
|---------|----------|
| "Found 0 users in voice" | Enable intents in Discord Developer Portal |
| Scrambled OLED text | Reflash latest firmware with padding fixes |
| Scripts won't start | Check absolute paths in .bat files |
| Can't compile | Use QMK MSYS, not Windows CMD |

### GitHub Upload

```bash
cd C:\Users\Renobatio\qmk_firmware\keyboards\nullbitsco\tidbit
git init
git add .
git commit -m "Initial commit"
git remote add origin https://github.com/OffReno/custom-qmk-tidbit-firmware.git
git push -u origin main
```

See [GITHUB_SETUP.md](GITHUB_SETUP.md) for detailed instructions.

### File Structure At A Glance

```
tidbit/
├── keyboard.json           # Hardware config
├── rules.mk               # Build rules
├── config.h               # Feature config
├── tidbit.c               # Core keyboard code
├── keymaps/default/
│   ├── keymap.c           # Your custom firmware ⭐
│   ├── *.py               # Python scripts (3 files)
│   ├── *.bat              # Windows batch files (~14 files)
│   ├── discord_config.txt # YOUR CREDENTIALS (not in git!)
│   └── README.md          # Full documentation
└── .gitignore             # Protects sensitive files
```

---

**For full documentation, see [README.md](README.md)**

**For GitHub setup, see [GITHUB_SETUP.md](GITHUB_SETUP.md)**
