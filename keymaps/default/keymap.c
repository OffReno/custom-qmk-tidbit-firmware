/* Copyright 2021 Jay Greco
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "raw_hid.h"

enum layers {
    _BASE = 0,
    _FUNC
};

enum custom_keycodes {
    OPEN_STEAM = SAFE_RANGE,
    TOGGLE_MONITOR,  // Toggle system monitoring
    TOGGLE_LIFX      // Toggle LIFX control script
};

// Encoder state tracking
typedef enum {
    MODE_START = 0,  // Clockwise - start apps
    MODE_KILL = 1    // Counterclockwise - kill apps
} encoder_mode_t;

static encoder_mode_t current_mode = MODE_START;
static int8_t app_index = 0;  // Current selected app index (-1 = idle)
static uint32_t last_encoder_time = 0;  // Last time encoder was turned
static bool pending_action = false;  // Action waiting to be executed
static bool monitoring_active = false;  // System monitoring state
static uint32_t monitoring_start_time = 0;  // Time when monitoring was activated
static bool monitoring_startup = false;  // Whether we're in the 5-second startup phase
static bool volume_balance_running = false;  // Volume balance script state
static bool discord_control_running = false;  // Discord voice control state
static bool lifx_control_running = false;  // LIFX lamp control state

// System monitor data from HID RAW
static uint8_t cpu_load = 0;
static uint8_t gpu_load = 0;
static uint16_t fps = 0;
static uint16_t ping = 0;

// Discord voice control data from HID RAW
static char discord_user[28] = "No users";  // Current selected user name
static uint8_t discord_user_index = 0;      // Current user index
static uint8_t discord_user_total = 0;      // Total users in voice
static bool discord_user_muted = false;     // Mute status of selected user
static uint32_t discord_message_time = 0;   // Time when temporary message was shown
static char discord_message[28] = "";       // Temporary message buffer
static bool discord_showing_message = false; // Flag to keep showing Discord display for messages

// LIFX lamp control data from HID RAW
static char lifx_message[28] = "";          // LIFX status message
static uint32_t lifx_message_time = 0;      // Time when LIFX message was shown
static bool lifx_showing_message = false;   // Flag to show LIFX message

// Volume balancer control data
static char volume_message[28] = "";        // Volume balancer status message
static uint32_t volume_message_time = 0;    // Time when volume message was shown
static bool volume_showing_message = false; // Flag to show volume message

// RGB LED control data
static char rgb_message[28] = "";           // RGB LED status message
static uint32_t rgb_message_time = 0;       // Time when RGB message was shown
static bool rgb_showing_message = false;    // Flag to show RGB message
static uint8_t current_color_index = 0;     // Track current color (0-8)

// Power control confirmation system
static uint8_t power_action_pending = 0;    // 0=none, 1=shutdown, 2=hibernate, 3=restart
static uint32_t power_action_time = 0;      // Time when power action was first pressed
static char power_message[28] = "";         // Power action message line 1 for OLED
static char power_message2[28] = "";        // Power action message line 2 for OLED
static bool power_showing_message = false;  // Flag to show power confirmation message

// App names for starting (clockwise)
static const char* start_apps[] = {"Steam", "Discord", "Desktop WP", "NordVPN"};
static const char* start_bat_files[] = {
    "start_steam.bat",
    "start_discord.bat",
    "start_wallpaper.bat",
    "start_nordvpn.bat"
};
#define NUM_START_APPS 4

// App names for killing (counterclockwise)
static const char* kill_apps[] = {"Steam", "Discord", "Desktop WP", "NordVPN"};
static const char* kill_bat_files[] = {
    "kill_steam.bat",
    "kill_discord.bat",
    "kill_wallpaper.bat",
    "kill_nordvpn.bat"
};
#define NUM_KILL_APPS 4

// Track last opened app for OLED display
static const char* last_app = "Idle";

// RGB LED mode names for OLED display
static const char* rgb_mode_names[] = {
    "Static",
    "Breathing",
    "Rainbow Mood",
    "Christmas",
    "RGB Test",
    "Alternating"
};
#define NUM_RGB_MODES 6

// RGB Color names for OLED display (hue-based)
static const char* rgb_color_names[] = {
    "Red",
    "Orange",
    "Yellow",
    "Green",
    "Cyan",
    "Blue",
    "Purple",
    "Magenta",
    "White"
};
#define NUM_RGB_COLORS 9

// Encoder button detection via matrix
// The encoder button is wired to the keyboard matrix at the KC_P7 position
// We'll intercept this keypress to toggle monitoring mode

#ifdef OLED_ENABLE
static bool oled_initialized = false;

// Bitmap logos (128x32 pixels each)
static const char PROGMEM logo_steam[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xf8,
	0xfe, 0xc7, 0x99, 0x7d, 0x7d, 0x7d, 0x3d, 0x99, 0xc7, 0xfe, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0f, 0x4f, 0x1f, 0x1f, 0x3e, 0x3e, 0x7c, 0xfc, 0xfc, 0xfc, 0xfe, 0xf7, 0x27, 0xdf, 0x7f,
	0x1f, 0x0f, 0x07, 0x07, 0x03, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x01, 0x03, 0x02, 0x03, 0x01, 0x41, 0x40,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM logo_desktop_wp[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0x60, 0x60, 0x60, 
    0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xfc, 0x0c, 0x8e, 0xcc, 0xec, 0x6c, 0x0e, 0x8c, 0xce, 
    0xef, 0xef, 0xce, 0x8c, 0x1c, 0xfe, 0xfc, 0xc0, 0xc0, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x01, 0x01, 0x13, 0x3e, 0x18, 0x11, 0x33, 
    0x67, 0x67, 0x63, 0x21, 0x24, 0x27, 0xa7, 0x23, 0x21, 0x30, 0x1f, 0x0f, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 
    0x06, 0x06, 0x06, 0x06, 0x06, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM logo_discord[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xf8, 0xfc, 0xfe, 0xfe, 0xff, 0x7f, 0x7f, 0xfe,
    0xfe, 0xfe, 0xfe, 0x7f, 0x7f, 0xff, 0xfe, 0xfe, 0xfc, 0xf8, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x3f, 0x7f, 0x7f, 0x7f, 0x7e, 0x1c, 0x3c, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3c, 0x1c, 0x7e, 0x7f, 0x7f, 0x7f, 0x3f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM logo_nordvpn[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xc0, 0xe0, 0xf0, 0xf0, 0xf8, 0xfc, 0xfc, 0xfc, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
    0xfe, 0xfe, 0xfe, 0xfc, 0xfc, 0xfc, 0xf8, 0xf0, 0xf0, 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xfc,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x1f, 0x0f, 0x0f, 0x1f,
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xf0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x3f, 0x0f, 0x07, 0x01, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x03, 0x00, 0x01, 0x07, 0x0f, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x07, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x07, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM my_logo[] = {

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0x7c, 0x1c, 0x0e, 0x06, 0x06, 0x07, 0x06, 0x06, 0x0e, 0x1c, 
    0xfc, 0xf8, 0xe0, 0x00, 0x00, 0x40, 0x60, 0xfc, 0xfe, 0xfe, 0x66, 0x46, 0x00, 0x40, 0x60, 0xfc, 
    0xfe, 0xfe, 0x66, 0x46, 0x00, 0x00, 0x00, 0xfe, 0xfe, 0xfe, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 
    0x0e, 0xde, 0xfc, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0x60, 0x60, 0x60, 0xe0, 0xc0, 
    0x80, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xe0, 0xe0, 0x80, 0xc0, 0x60, 0x60, 0xe0, 0xe0, 0xc0, 0x80, 
    0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xc0, 0xe0, 0xe0, 0x60, 0xe0, 0xe0, 0xc0, 0x80, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x0f, 0x3f, 0xff, 0xf0, 0xc0, 0x80, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0xc0, 
    0xf8, 0x7f, 0x3f, 0x02, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 
    0x0f, 0xff, 0xfd, 0xf8, 0x00, 0x00, 0x00, 0x7e, 0xff, 0xff, 0x8c, 0x0c, 0x0c, 0x0c, 0x8c, 0x8f, 
    0xcf, 0x4f, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 
    0x00, 0x00, 0x00, 0x3e, 0xff, 0xff, 0xc1, 0x80, 0x00, 0x00, 0x00, 0x80, 0xf7, 0xff, 0x7f, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 
    0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// HID RAW receive callback - receives data from Python script
void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (data[0] == 0xF0) {  // System monitor data packet
        cpu_load = data[1];
        gpu_load = data[2];
        fps = (data[3] << 8) | data[4];
        ping = (data[5] << 8) | data[6];
    }
    else if (data[0] == 0xF2 && data[1] == 0x01) {  // Discord user update
        discord_user_index = data[2];
        discord_user_total = data[3];

        // Extract username from remaining bytes (max 27 chars)
        uint8_t name_len = 0;
        for (uint8_t i = 4; i < length && i < 31 && data[i] != 0; i++) {
            discord_user[name_len++] = data[i];
        }
        discord_user[name_len] = '\0';  // Null terminate
    }
    else if (data[0] == 0xF2 && data[1] == 0x04) {  // Discord mute status update
        discord_user_muted = data[2];  // 0 = unmuted, 1 = muted

        // Set temporary message with mute status (keep it short - max 21 chars)
        if (discord_user_muted) {
            // Truncate username if too long
            char short_name[15];
            strncpy(short_name, discord_user, 14);
            short_name[14] = '\0';
            snprintf(discord_message, sizeof(discord_message), "%s MUTED", short_name);
        } else {
            char short_name[15];
            strncpy(short_name, discord_user, 14);
            short_name[14] = '\0';
            snprintf(discord_message, sizeof(discord_message), "%s UNMUTED", short_name);
        }
        discord_message_time = timer_read32();  // Mark message time for 3-second display
        discord_showing_message = true;
    }
    else if (data[0] == 0xF3 && data[1] == 0x04) {  // LIFX status message
        // Extract message from remaining bytes (max 27 chars)
        uint8_t msg_len = 0;
        for (uint8_t i = 2; i < length && i < 29 && data[i] != 0; i++) {
            lifx_message[msg_len++] = data[i];
        }
        lifx_message[msg_len] = '\0';  // Null terminate
        lifx_message_time = timer_read32();  // Mark message time for 5-second display
        lifx_showing_message = true;
    }
}

// Helper function to write large text using multiple rows (for apps without bitmaps yet)
void render_text_large(const char* text) {
    // Write text on all 4 lines to make it appear larger/bolder
    for (uint8_t i = 0; i < 4; i++) {
        oled_set_cursor(0, i);

        // Add spacing to center text
        if (strcmp(text, "Ready") == 0) {
            oled_write_P(PSTR("      READY       "), false);
        } else if (strcmp(text, "Discord") == 0) {
            oled_write_P(PSTR("     DISCORD      "), false);
        } else if (strcmp(text, "Closed WP") == 0) {
            oled_write_P(PSTR("    CLOSED WP     "), false);
        } else if (strcmp(text, "Closed VPN") == 0) {
            oled_write_P(PSTR("   CLOSED VPN     "), false);
        } else if (strcmp(text, "Monitoring") == 0) {
            oled_write_P(PSTR("   MONITORING     "), false);
        } else if (strcmp(text, "NordVPN") == 0) {
            oled_write_P(PSTR("     NORDVPN      "), false);
        }
    }
}

// Main render function - uses bitmaps where available, text otherwise
void render_large_text(const char* text) {
    if (strcmp(text, "Steam") == 0) {
        // Use bitmap for Steam
        oled_write_raw_P(logo_steam, sizeof(logo_steam));
    } else if (strcmp(text, "Discord") == 0) {
        // Use bitmap for Discord
        oled_write_raw_P(logo_discord, sizeof(logo_discord));
    } else if (strcmp(text, "Desktop WP") == 0 || strcmp(text, "Closed WP") == 0) {
        // Use bitmap for Wallpaper Engine
        oled_write_raw_P(logo_desktop_wp, sizeof(logo_desktop_wp));
    } else if (strcmp(text, "NordVPN") == 0 || strcmp(text, "Closed VPN") == 0) {
        // Use bitmap for NordVPN
        oled_write_raw_P(logo_nordvpn, sizeof(logo_nordvpn));
    } else if (strcmp(text, "Idle") == 0) {
        // Use my custom idle logo
        oled_write_raw_P(my_logo, sizeof(my_logo));
    } else {
        // Use text rendering for others
        render_text_large(text);
    }
}
#endif

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [_BASE] = LAYOUT(
  KC_PSLS, KC_PAST, KC_PMNS,
  KC_P7, KC_P8,   KC_P9,   KC_PPLS,
  KC_P4, KC_P5,   KC_P6,   KC_PPLS,
  KC_P1, KC_P2,   KC_P3,   KC_PENT,
  KC_P0, TOGGLE_LIFX, KC_PDOT, KC_PENT
  ),

    [_FUNC] = LAYOUT(
    _______, _______, _______,
    _______, _______, _______, _______,
    _______, _______, _______, _______,
    _______, _______, _______, _______,
    _______, _______, _______, _______
    )
};

// Helper function to execute bat file via Win+R
void execute_bat_file(const char* bat_file) {
    tap_code16(LGUI(KC_R));  // Win+R to open Run dialog
    wait_ms(150);

    // Clear any existing text
    tap_code16(LCTL(KC_A));
    wait_ms(50);

    // Type the full path to the bat file
    send_string("C:\\Users\\Renobatio\\qmk_firmware\\keyboards\\nullbitsco\\tidbit\\keymaps\\default\\");
    send_string(bat_file);

    wait_ms(50);
    tap_code(KC_ENT);
}

// Handle custom keycodes
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case KC_PSLS:  // '/' key - Shutdown PC
            if (record->event.pressed) {
                if (power_action_pending == 1) {
                    // Second press - cancel shutdown
                    power_action_pending = 0;
                    power_action_time = 0;
                    power_showing_message = false;
                    strcpy(power_message, "Shutdown Cancelled");
                    power_message2[0] = '\0';  // Clear second line
                    power_action_time = timer_read32();
                    power_showing_message = true;
                } else {
                    // First press - show confirmation
                    power_action_pending = 1;  // Shutdown
                    power_action_time = timer_read32();
                    strcpy(power_message, "Press again to cancel :");
                    strcpy(power_message2, "Shutdown");
                    power_showing_message = true;
                }
            }
            return false;  // Prevent '/' from being sent

        case KC_PAST:  // '*' key - Hibernate PC
            if (record->event.pressed) {
                if (power_action_pending == 2) {
                    // Second press - cancel hibernate
                    power_action_pending = 0;
                    power_action_time = 0;
                    power_showing_message = false;
                    strcpy(power_message, "Hibernate Cancelled");
                    power_message2[0] = '\0';  // Clear second line
                    power_action_time = timer_read32();
                    power_showing_message = true;
                } else {
                    // First press - show confirmation
                    power_action_pending = 2;  // Hibernate
                    power_action_time = timer_read32();
                    strcpy(power_message, "Press again to cancel :");
                    strcpy(power_message2, "Hibernate");
                    power_showing_message = true;
                }
            }
            return false;  // Prevent '*' from being sent

        case KC_PMNS:  // '-' key - Restart PC
            if (record->event.pressed) {
                if (power_action_pending == 3) {
                    // Second press - cancel restart
                    power_action_pending = 0;
                    power_action_time = 0;
                    power_showing_message = false;
                    strcpy(power_message, "Restart Cancelled");
                    power_message2[0] = '\0';  // Clear second line
                    power_action_time = timer_read32();
                    power_showing_message = true;
                } else {
                    // First press - show confirmation
                    power_action_pending = 3;  // Restart
                    power_action_time = timer_read32();
                    strcpy(power_message, "Press again to cancel :");
                    strcpy(power_message2, "Restart");
                    power_showing_message = true;
                }
            }
            return false;  // Prevent '-' from being sent

        case KC_P7:  // Encoder 0 button - toggle monitoring
            if (record->event.pressed) {
                // Toggle monitoring mode when encoder button is pressed
                monitoring_active = !monitoring_active;
                if (monitoring_active) {
                    // Start monitoring - begin 5 second startup phase
                    monitoring_startup = true;
                    monitoring_start_time = timer_read32();
                    execute_bat_file("start_monitor.bat");
                    last_app = "Monitoring";
                    // Cancel any pending encoder rotation action
                    pending_action = false;
                } else {
                    // Stop monitoring
                    monitoring_startup = false;
                    execute_bat_file("kill_monitor.bat");
                    last_app = "Idle";
                    // Reset monitoring data
                    cpu_load = 0;
                    gpu_load = 0;
                    fps = 0;
                    ping = 0;
                }
            }
            return false;  // Prevent KC_P7 from being sent

        case KC_P4:  // Encoder 1 button - rebalance volumes
            if (record->event.pressed) {
                // Send HID RAW command to rebalance Discord/Game volumes
                uint8_t data[32] = {0};
                data[0] = 0xF1;  // Volume command identifier
                data[1] = 0x03;  // 0x03 = Rebalance command
                raw_hid_send(data, sizeof(data));
            }
            return false;  // Prevent KC_P4 from being sent

        case KC_P5:  // Toggle volume balance script
            if (record->event.pressed) {
                volume_balance_running = !volume_balance_running;
                if (volume_balance_running) {
                    execute_bat_file("start_volume.bat");
                    strcpy(volume_message, "Volume Balancer ON");
                    volume_message_time = timer_read32();
                    volume_showing_message = true;
                } else {
                    execute_bat_file("kill_volume.bat");
                    strcpy(volume_message, "Volume Balancer OFF");
                    volume_message_time = timer_read32();
                    volume_showing_message = true;
                }
            }
            return false;  // Prevent KC_P5 from being sent

        case KC_P8:  // '8' key - Cycle RGB LED modes
            if (record->event.pressed) {
                rgblight_step();  // Cycle to next mode
                uint8_t mode = rgblight_get_mode();
                // Mode numbers start at 1, array starts at 0
                if (mode >= 1 && mode <= NUM_RGB_MODES) {
                    snprintf(rgb_message, sizeof(rgb_message), "LED: %s", rgb_mode_names[mode - 1]);
                } else {
                    strcpy(rgb_message, "LED Mode Changed");
                }
                rgb_message_time = timer_read32();
                rgb_showing_message = true;
            }
            return false;  // Prevent '8' from being sent

        case KC_P9:  // '9' key - Cycle RGB LED colors
            if (record->event.pressed) {
                // Increment color index (0-8, then wrap to 0)
                current_color_index = (current_color_index + 1) % NUM_RGB_COLORS;

                // Get current brightness
                uint8_t brightness = rgblight_get_val();

                if (current_color_index == 8) {
                    // White: any hue, saturation = 0
                    rgblight_sethsv(0, 0, brightness);
                } else {
                    // Hue-based colors: hue = index * 32, saturation = 255
                    uint8_t hue = current_color_index * 32;
                    rgblight_sethsv(hue, 255, brightness);
                }

                // Show color name on OLED
                snprintf(rgb_message, sizeof(rgb_message), "Color: %s", rgb_color_names[current_color_index]);
                rgb_message_time = timer_read32();
                rgb_showing_message = true;
            }
            return false;  // Prevent '9' from being sent

        case KC_P1:  // Encoder 2 button - Mute/unmute only
            if (record->event.pressed && discord_control_running) {
                // Send mute/unmute command for selected user
                uint8_t data[32] = {0};
                data[0] = 0xF2;  // Discord command identifier
                data[1] = 0x03;  // 0x03 = Mute/unmute command
                raw_hid_send(data, sizeof(data));
            }
            return false;  // Prevent KC_P1 from being sent

        case KC_P2:  // Toggle Discord voice control
            if (record->event.pressed) {
                discord_control_running = !discord_control_running;
                if (discord_control_running) {
                    // Start Discord bot
                    execute_bat_file("start_discord.bat");
                    strcpy(discord_message, "DICTATOR MODE ON");
                    discord_message_time = timer_read32();
                    discord_showing_message = true;
                    discord_user_index = 0;
                    discord_user_total = 0;
                    strcpy(discord_user, "Starting...");
                } else {
                    // Stop Discord bot
                    execute_bat_file("kill_discord.bat");
                    strcpy(discord_message, "DEMOCRACY RESTORED");
                    discord_message_time = timer_read32();
                    discord_showing_message = true;  // Keep showing Discord display for shutdown message
                }
            }
            return false;  // Prevent KC_P2 from being sent

        case KC_P0:  // Encoder 3 button - Toggle LIFX lamp on/off
            if (record->event.pressed) {
                // Send toggle command to LIFX control script
                uint8_t data[32] = {0};
                data[0] = 0xF3;  // LIFX command identifier
                data[1] = 0x03;  // 0x03 = Toggle lamp on/off command
                raw_hid_send(data, sizeof(data));
            }
            return false;  // Prevent KC_P0 from being sent

        case TOGGLE_LIFX:  // Toggle LIFX control script
            if (record->event.pressed) {
                lifx_control_running = !lifx_control_running;
                if (lifx_control_running) {
                    // Start LIFX control script
                    execute_bat_file("start_lifx.bat");
                } else {
                    // Stop LIFX control script
                    execute_bat_file("kill_lifx.bat");
                }
            }
            return false;

        case TOGGLE_MONITOR:
            if (record->event.pressed) {
                monitoring_active = !monitoring_active;
                if (monitoring_active) {
                    // Start monitoring - begin 5 second startup phase
                    monitoring_startup = true;
                    monitoring_start_time = timer_read32();
                    execute_bat_file("start_monitor.bat");
                    last_app = "Monitoring";
                    // Cancel any pending encoder rotation action
                    pending_action = false;
                } else {
                    // Stop monitoring
                    monitoring_startup = false;
                    execute_bat_file("kill_monitor.bat");
                    last_app = "Idle";
                    // Reset monitoring data
                    cpu_load = 0;
                    gpu_load = 0;
                    fps = 0;
                    ping = 0;
                }
            }
            return false;
    }
    return true;
}

// Custom encoder rotation handler
bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) { // First encoder - app control
        // Don't process rotation if monitoring is active
        if (monitoring_active) {
            return false;
        }

        // Update mode based on direction
        if (clockwise) {
            current_mode = MODE_START;
            // Cycle through start apps
            app_index++;
            if (app_index >= NUM_START_APPS) {
                app_index = 0;
            }
            last_app = start_apps[app_index];
        } else {
            current_mode = MODE_KILL;
            // Cycle through kill apps
            app_index++;
            if (app_index >= NUM_KILL_APPS) {
                app_index = 0;
            }
            last_app = kill_apps[app_index];
        }

        // Mark that we have a pending action and reset timer
        pending_action = true;
        last_encoder_time = timer_read32();

        return false; // Skip default encoder behavior
    }
    else if (index == 1) { // Second encoder - volume balancing
        // Send HID RAW command for volume control
        uint8_t data[32] = {0};
        data[0] = 0xF1;  // Volume command identifier
        data[1] = clockwise ? 0x01 : 0x02;  // 0x01=CW (game up), 0x02=CCW (discord up)

        raw_hid_send(data, sizeof(data));
        return false;
    }
    else if (index == 2) { // Third encoder - Discord voice control
        // Send HID RAW command for Discord user cycling
        uint8_t data[32] = {0};
        data[0] = 0xF2;  // Discord command identifier
        data[1] = clockwise ? 0x01 : 0x02;  // 0x01=CW (next user), 0x02=CCW (prev user)

        raw_hid_send(data, sizeof(data));
        return false;
    }
    else if (index == 3) { // Fourth encoder - LIFX lamp brightness
        // Send HID RAW command for LIFX brightness control
        uint8_t data[32] = {0};
        data[0] = 0xF3;  // LIFX command identifier
        data[1] = clockwise ? 0x01 : 0x02;  // 0x01=CW (brightness up), 0x02=CCW (brightness down)

        raw_hid_send(data, sizeof(data));
        return false;
    }
    return true; // Continue with default behavior for other encoders
}

// Initialize keyboard
void keyboard_post_init_user(void) {
    // Set RGB LED brightness to 100% (255 out of 255)
    rgblight_sethsv(0, 255, 255);  // Red color at 100% brightness
    rgblight_enable();  // Ensure RGB is enabled
}

// Matrix scan for 2-second timeout and monitoring startup
void matrix_scan_user(void) {
    // Handle pending app launch/kill actions (only when not monitoring)
    if (pending_action && !monitoring_active) {
        // Check if 2 seconds have elapsed since last encoder turn
        if (timer_elapsed32(last_encoder_time) >= 2000) {
            // Execute the appropriate bat file
            if (current_mode == MODE_START) {
                execute_bat_file(start_bat_files[app_index]);
            } else {
                execute_bat_file(kill_bat_files[app_index]);
            }

            // Clear pending action and reset to idle
            pending_action = false;
            app_index = -1;
            last_app = "Idle";
        }
    }
    
    // Handle monitoring startup phase (5 seconds)
    if (monitoring_startup && monitoring_active) {
        if (timer_elapsed32(monitoring_start_time) >= 5000) {
            // 5 seconds elapsed, exit startup phase
            monitoring_startup = false;
        }
    }

    // Handle power action timeout (5 seconds)
    if (power_action_pending != 0) {
        if (timer_elapsed32(power_action_time) >= 5000) {
            // 5 seconds elapsed - execute action
            if (power_action_pending == 1) {
                // Shutdown
                tap_code16(LGUI(KC_R));
                wait_ms(150);
                tap_code16(LCTL(KC_A));
                wait_ms(50);
                send_string("shutdown /s /t 0");
                wait_ms(50);
                tap_code(KC_ENT);
            } else if (power_action_pending == 2) {
                // Hibernate
                tap_code16(LGUI(KC_R));
                wait_ms(150);
                tap_code16(LCTL(KC_A));
                wait_ms(50);
                send_string("shutdown /h");
                wait_ms(50);
                tap_code(KC_ENT);
            } else if (power_action_pending == 3) {
                // Restart
                tap_code16(LGUI(KC_R));
                wait_ms(150);
                tap_code16(LCTL(KC_A));
                wait_ms(50);
                send_string("shutdown /r /t 0");
                wait_ms(50);
                tap_code(KC_ENT);
            }
            // Clear pending action
            power_action_pending = 0;
            power_action_time = 0;
            power_showing_message = false;
        }
    }
}

#ifdef OLED_ENABLE
// Custom OLED display - show app name in large format across whole screen
bool oled_task_user(void) {
    // State tracking for clearing only when needed (prevents flickering)
    static bool last_monitoring_active = false;
    static bool last_monitoring_startup = false;
    static bool last_discord_active = false;
    static bool last_lifx_active = false;
    static bool last_volume_active = false;
    static bool last_rgb_active = false;
    static bool last_power_active = false;

    // Clear display on first run only
    if (!oled_initialized) {
        oled_clear();
        oled_initialized = true;
    }

    // Check if we need to clear the display (only on state changes)
    bool should_clear = false;
    bool current_discord_display = discord_control_running || discord_showing_message;
    if (monitoring_active != last_monitoring_active || current_discord_display != last_discord_active ||
        lifx_showing_message != last_lifx_active || volume_showing_message != last_volume_active ||
        rgb_showing_message != last_rgb_active || power_showing_message != last_power_active) {
        should_clear = true;  // Mode changed
    } else if (monitoring_active && (monitoring_startup != last_monitoring_startup)) {
        should_clear = true;  // Within monitoring, startup phase changed
    }

    if (should_clear) {
        oled_clear();
    }

    // Update state tracking
    last_monitoring_active = monitoring_active;
    last_monitoring_startup = monitoring_startup;
    last_discord_active = current_discord_display;
    last_lifx_active = lifx_showing_message;
    last_volume_active = volume_showing_message;
    last_rgb_active = rgb_showing_message;
    last_power_active = power_showing_message;

    // Priority 1: Power confirmation messages (highest priority)
    if (power_showing_message) {
        char buf[22];

        // Check if cancellation message should clear after 3 seconds
        if (power_action_pending == 0 && power_action_time != 0 && timer_elapsed32(power_action_time) >= 3000) {
            // Clear cancellation message after 3 seconds
            power_message[0] = '\0';
            power_message2[0] = '\0';
            power_action_time = 0;
            power_showing_message = false;
            oled_clear();
            render_large_text(last_app);
        } else {
            // Show power confirmation message (two lines)
            oled_set_cursor(0, 0);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 1);
            snprintf(buf, sizeof(buf), "%-21s", power_message);
            oled_write(buf, false);

            oled_set_cursor(0, 2);
            snprintf(buf, sizeof(buf), "%-21s", power_message2);
            oled_write(buf, false);

            oled_set_cursor(0, 3);
            oled_write_P(PSTR("                     "), false);
        }
    }
    // Priority 2: If in monitoring mode
    else if (monitoring_active) {
        if (monitoring_startup) {
            // Show "Monitoring" with dots based on elapsed time
            uint32_t elapsed = timer_elapsed32(monitoring_start_time);
            uint8_t dots = (elapsed / 1000);  // 1 dot per second
            if (dots > 5) dots = 5;

            oled_set_cursor(0, 1);  // Center vertically
            oled_write_P(PSTR("  Monitoring"), false);
            for (uint8_t i = 0; i < dots; i++) {
                oled_write_P(PSTR("."), false);
            }
        } else {
            // Show system stats
            char buf[22];

            // Line 0: CPU
            oled_set_cursor(0, 0);
            snprintf(buf, sizeof(buf), "CPU: %3d%%", cpu_load);
            oled_write(buf, false);

            // Line 1: GPU
            oled_set_cursor(0, 1);
            snprintf(buf, sizeof(buf), "GPU: %3d%%", gpu_load);
            oled_write(buf, false);

            // Line 2: GPU Memory (VRAM)
            oled_set_cursor(0, 2);
            snprintf(buf, sizeof(buf), "MEM: %3d%%", fps);
            oled_write(buf, false);

            // Line 3: Ping
            oled_set_cursor(0, 3);
            snprintf(buf, sizeof(buf), "MS : %3dms", ping);
            oled_write(buf, false);
        }
    } else if (discord_control_running || discord_showing_message) {
        // Discord voice control mode - show selected user or temporary message
        char buf[22];

        // Check if we have a temporary message to show (within 3 seconds)
        static bool last_showing_temp = false;
        bool show_temp_message = false;
        if (discord_message_time != 0 && timer_elapsed32(discord_message_time) < 3000) {
            show_temp_message = true;
        } else if (discord_message_time != 0) {
            // Clear message after 3 seconds
            discord_message[0] = '\0';
            discord_message_time = 0;
            discord_showing_message = false;
            oled_clear();  // Clear when transitioning from temp message
        }

        // Clear display when switching between temp and normal
        if (show_temp_message != last_showing_temp) {
            oled_clear();
        }
        last_showing_temp = show_temp_message;

        if (show_temp_message) {
            // Show temporary message (startup/shutdown/mute status)
            // Clear all lines with padding
            oled_set_cursor(0, 0);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 1);
            snprintf(buf, sizeof(buf), "%-21s", discord_message);
            oled_write(buf, false);

            oled_set_cursor(0, 2);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 3);
            oled_write_P(PSTR("                     "), false);
        } else {
            // Show normal Discord user info
            // Line 0: Title (pad to 21 chars)
            oled_set_cursor(0, 0);
            oled_write_P(PSTR("Discord Voice        "), false);

            // Line 1: User count (pad to 21 chars)
            oled_set_cursor(0, 1);
            if (discord_user_total > 0) {
                snprintf(buf, sizeof(buf), "[%d/%d]                  ", discord_user_index + 1, discord_user_total);
            } else {
                snprintf(buf, sizeof(buf), "No users             ");
            }
            oled_write(buf, false);

            // Line 2: Username (pad to 21 chars)
            oled_set_cursor(0, 2);
            snprintf(buf, sizeof(buf), "%-21s", discord_user);
            oled_write(buf, false);

            // Line 3: Clear it (pad to 21 chars)
            oled_set_cursor(0, 3);
            oled_write_P(PSTR("                     "), false);
        }
    } else if (lifx_showing_message) {
        // LIFX status message mode - show message for 5 seconds then return to logo
        char buf[22];

        // Check if we should still show the message (within 5 seconds)
        if (lifx_message_time != 0 && timer_elapsed32(lifx_message_time) < 5000) {
            // Show LIFX status message
            oled_set_cursor(0, 0);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 1);
            snprintf(buf, sizeof(buf), "%-21s", lifx_message);
            oled_write(buf, false);

            oled_set_cursor(0, 2);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 3);
            oled_write_P(PSTR("                     "), false);
        } else {
            // Clear message after 5 seconds and return to logo
            lifx_message[0] = '\0';
            lifx_message_time = 0;
            lifx_showing_message = false;
            oled_clear();
            // Show logo
            render_large_text(last_app);
        }
    } else if (volume_showing_message) {
        // Volume balancer status message mode - show message for 3 seconds then return to logo
        char buf[22];

        // Check if we should still show the message (within 3 seconds)
        if (volume_message_time != 0 && timer_elapsed32(volume_message_time) < 3000) {
            // Show volume balancer status message
            oled_set_cursor(0, 0);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 1);
            snprintf(buf, sizeof(buf), "%-21s", volume_message);
            oled_write(buf, false);

            oled_set_cursor(0, 2);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 3);
            oled_write_P(PSTR("                     "), false);
        } else {
            // Clear message after 3 seconds and return to logo
            volume_message[0] = '\0';
            volume_message_time = 0;
            volume_showing_message = false;
            oled_clear();
            // Show logo
            render_large_text(last_app);
        }
    } else if (rgb_showing_message) {
        // RGB LED status message mode - show message for 3 seconds then return to logo
        char buf[22];

        // Check if we should still show the message (within 3 seconds)
        if (rgb_message_time != 0 && timer_elapsed32(rgb_message_time) < 3000) {
            // Show RGB status message
            oled_set_cursor(0, 0);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 1);
            snprintf(buf, sizeof(buf), "%-21s", rgb_message);
            oled_write(buf, false);

            oled_set_cursor(0, 2);
            oled_write_P(PSTR("                     "), false);

            oled_set_cursor(0, 3);
            oled_write_P(PSTR("                     "), false);
        } else {
            // Clear message after 3 seconds and return to logo
            rgb_message[0] = '\0';
            rgb_message_time = 0;
            rgb_showing_message = false;
            oled_clear();
            // Show logo
            render_large_text(last_app);
        }
    } else {
        // Normal mode - render large text filling the screen
        render_large_text(last_app);
    }

    return false;  // Return false to prevent oled_task_kb from running
}

// Override the keyboard-level OLED init to prevent logo rendering
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_180;
}

// Keep OLED always on - disable timeout
uint16_t oled_timeout_callback(uint16_t timeout) {
    return 0;  // Return 0 to disable timeout
}
#endif