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
#pragma once

// Disable OLED timeout (keep display always on)
#define OLED_TIMEOUT 0

#define USB_SUSPEND_WAKEUP_DELAY 200

// #define ENCODER_BUTTON_ENABLE
// ENCODER_BUTTON_PIN_0 is commented out because C7 is AVR-only
// For RP2040, you need to use GPIO pins like GP10, GP11, etc.
// Uncomment and set to correct RP2040 GPIO pin once you verify your hardware wiring:
// #define ENCODER_BUTTON_PIN_0 GP10

