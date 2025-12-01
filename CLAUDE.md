# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

**User Environment**: Windows
**Hardware Setup**: Custom "OffReno" build based on TIDBIT design
- **MCU**: Frood RP2040 (not native Pro Micro)
- **Display**: SSD1306 OLED (128x32) connected via I2C
- **Layout**:
  - First column (4 positions): Rotary encoders
  - Remaining columns: Switches/buttons (15 keys)
- **RGB Lighting**: WS2812, 8 LEDs

## Overview

This is a QMK firmware configuration for the **TIDBIT** keyboard - a 19-key numpad/macropad by nullbits, customized for RP2040 hardware.

## Build Commands

From the QMK root directory (`qmk_firmware/`):

```bash
# Build for Frood RP2040 (REQUIRED for this build)
make nullbitsco/tidbit:default CONVERT_TO=promicro_rp2040

# Build and flash to Frood RP2040
make nullbitsco/tidbit:default:flash CONVERT_TO=promicro_rp2040

# Clean build artifacts
make clean
```

**Important**:
- All build commands must be run from the QMK firmware root directory, not from this keyboard directory
- **Always use `CONVERT_TO=promicro_rp2040`** since this build uses Frood RP2040, not the native ATmega32u4
- On Windows, use Git Bash, WSL, or MSYS2 to run make commands

## Architecture

### File Structure

- **keyboard.json** - Main keyboard configuration (USB IDs, matrix, features, encoder pins, RGB)
- **config.h** - Keyboard-level configuration overrides and optional features
- **rules.mk** - Build rules and feature flags
- **tidbit.c** - Keyboard initialization and core functionality
- **keymaps/** - User keymap configurations
  - **default/** - Factory default keymap with encoder support

### Key Components

#### Keyboard Initialization Flow
1. `matrix_init_kb()` ([tidbit.c:98](tidbit.c#L98)) - Sets Bit-C LED on, initializes remote KB functionality
2. `process_record_kb()` ([tidbit.c:80](tidbit.c#L80)) - Auto-enables NUM LOCK on first keypress
3. `led_update_ports()` ([tidbit.c:76](tidbit.c#L76)) - Updates Bit-C LED based on NUM LOCK state
4. `shutdown_kb()` ([tidbit.c:109](tidbit.c#L109)) - Cleanup on shutdown/bootloader jump

#### Hardware Dependencies

**Bit-C LED & Remote KB**: The original TIDBIT design uses custom hardware features (Bit-C LED helper, remote_kb over UART/TRRS) via the `common/` directory. These are **disabled** in [rules.mk](rules.mk#L4-L11) because:
- Only work with native Pro Micro/Bit-C builds
- Not compatible with RP2040 conversion
- Stub functions in [keymaps/default/stubs.c](keymaps/default/stubs.c) prevent compilation errors while doing nothing

**This OffReno build does NOT use these features** - the stubs ensure tidbit.c compiles without modification.

#### OLED Display (ENABLED)
OLED is **enabled** in this build ([keymaps/default/rules.mk:4-5](keymaps/default/rules.mk#L4-L5)):
- Renders custom nullbits logo via [tidbit.c:23-73](tidbit.c#L23-L73)
- SSD1306 driver for 128x32 display over I2C
- Rotation set to 180° in `oled_init_kb()`

#### Encoder Configuration (4 Encoders in First Column)
Four rotary encoders occupy the first column:
- Encoder pins defined in [keyboard.json:44-50](keyboard.json#L44-L50)
- Currently only encoder 0 is mapped (volume control) in default keymap
- Enable encoder mapping with `ENCODER_MAP_ENABLE = yes` in [keymaps/default/rules.mk:1](keymaps/default/rules.mk#L1)
- All 4 encoders available in first column, remaining 15 positions are switches/buttons

### Modifying Hardware Configuration

1. **Matrix/Pin Changes**: Edit [keyboard.json](keyboard.json) - modify `matrix_pins`, `encoder.rotary`, or `ws2812.pin`
2. **Feature Toggles**: Edit [keyboard.json](keyboard.json) `features` section or keymap-specific `rules.mk`
3. **USB Identifiers**: Change `usb.vid`/`usb.pid`/`keyboard_name` in [keyboard.json](keyboard.json)
   - Current keyboard name: "OffReno" (custom variant)

### RP2040-Specific Notes

This build uses **Frood RP2040**, requiring `CONVERT_TO=promicro_rp2040` on all build commands.

**Bootloader Entry**: Frood RP2040 uses double-tap reset or BOOTSEL button. Optionally enable bootloader LED indicator in [config.h:41-42](config.h#L41-L42):
```c
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_LED 17
```

**I2C for OLED**: QMK's RP2040 conversion handles I2C automatically when OLED is enabled - no manual I2C driver configuration needed.

## Creating Custom Keymaps

1. Copy `keymaps/default/` to `keymaps/yourname/`
2. Edit `keymap.c` - modify layer definitions and encoder_map (all 4 encoders available)
3. Edit `rules.mk` - enable/disable features as needed (keep stubs.c for RP2040 build)
4. Build: `make nullbitsco/tidbit:yourname CONVERT_TO=promicro_rp2040`

**Current Layout**:
- First column: 4 rotary encoders (encoder 0 = volume control in default keymap)
- Remaining positions: 15 switches/buttons (standard numpad layout)
