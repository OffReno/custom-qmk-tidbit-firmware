#!/usr/bin/env python3
"""
Discord Voice Control for QMK TIDBIT Keyboard
Controls Discord voice channel users via HID RAW commands from encoder 2

Encoder 2 Controls:
  CW rotation  = Next user in voice channel
  CCW rotation = Previous user in voice channel
  Button press = Mute/Unmute selected user

Setup:
1. Create a Discord bot at https://discord.com/developers/applications
2. Enable SERVER MEMBERS INTENT and PRESENCE INTENT
3. Invite bot with "Mute Members" and "View Channels" permissions
4. Create discord_config.txt with:
   BOT_TOKEN=your_bot_token_here
   GUILD_ID=your_server_id_here
"""

import discord
from discord.ext import commands
import asyncio
import hid
import os
import sys

# QMK HID RAW settings
VID = 0x6E61  # OffReno keyboard vendor ID
PID = 0x6064  # OffReno keyboard product ID

# Discord bot settings
CONFIG_FILE = os.path.join(os.path.dirname(__file__), "discord_config.txt")

# Global state
current_user_index = 0
voice_users = []
selected_user = None
bot = None
target_guild = None

def load_config():
    """Load Discord bot token and guild ID from config file"""
    if not os.path.exists(CONFIG_FILE):
        print(f"ERROR: Config file not found: {CONFIG_FILE}")
        print("\nCreate discord_config.txt with:")
        print("BOT_TOKEN=your_bot_token_here")
        print("GUILD_ID=your_server_id_here")
        sys.exit(1)

    config = {}
    with open(CONFIG_FILE, 'r') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, value = line.split('=', 1)
                config[key.strip()] = value.strip()

    if 'BOT_TOKEN' not in config or 'GUILD_ID' not in config:
        print("ERROR: Missing BOT_TOKEN or GUILD_ID in discord_config.txt")
        sys.exit(1)

    return config['BOT_TOKEN'], int(config['GUILD_ID'])

def find_keyboard():
    """Find the OffReno keyboard RAW HID interface"""
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            # QMK RAW HID uses usage_page 0xFF60 and usage 0x61
            if device.get('usage_page') == 0xFF60 and device.get('usage') == 0x61:
                print(f"Found RAW HID interface: {device.get('interface_number', 'N/A')}")
                return device['path']

    # Fallback
    print("RAW HID interface not found by usage page, trying fallback...")
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            print(f"Using interface: {device.get('interface_number', 'N/A')}")
            return device['path']

    return None

def get_voice_users(guild):
    """Get list of users in voice channels"""
    users = []
    print(f"\nScanning voice channels in {guild.name}...")
    print(f"Total voice channels: {len(guild.voice_channels)}")
    for channel in guild.voice_channels:
        print(f"  Channel: {channel.name} - Members: {len(channel.members)}")
        for member in channel.members:
            print(f"    - {member.display_name} (bot={member.bot})")
            if not member.bot:  # Skip bots
                users.append({
                    'name': member.display_name,
                    'member': member,
                    'channel': channel.name
                })
    print(f"Found {len(users)} non-bot users in voice")
    return users

def send_user_to_oled(keyboard, username, index, total):
    """Send username to keyboard for OLED display"""
    # Protocol: 0xF2 = Discord command, 0x01 = Update display
    # Format: [report_id, 0xF2, 0x01, index, total, username_bytes...]
    data = [0] * 33  # 33 bytes: 1 for report ID + 32 for data
    data[0] = 0x00  # Report ID (required for HID)
    data[1] = 0xF2  # Discord command identifier
    data[2] = 0x01  # Update display command
    data[3] = index  # Current user index
    data[4] = total  # Total users

    # Encode username (max 27 bytes to fit in remaining space)
    username_bytes = username[:27].encode('utf-8')
    for i, byte in enumerate(username_bytes):
        data[5 + i] = byte

    keyboard.write(bytes(data))
    print(f"Sent to OLED: [{index+1}/{total}] {username}")

async def update_voice_users():
    """Update the list of users in voice channels"""
    global voice_users, current_user_index, selected_user

    if not target_guild:
        return

    new_users = get_voice_users(target_guild)

    # Check if user list changed
    if len(new_users) != len(voice_users) or \
       any(u1['name'] != u2['name'] for u1, u2 in zip(new_users, voice_users)):
        voice_users = new_users

        # Reset index if out of bounds
        if current_user_index >= len(voice_users):
            current_user_index = 0

        # Update selected user
        if voice_users:
            selected_user = voice_users[current_user_index]
            print(f"\nVoice users updated: {len(voice_users)} users")
            for i, user in enumerate(voice_users):
                marker = ">" if i == current_user_index else " "
                print(f"  {marker} {user['name']} in {user['channel']}")
        else:
            selected_user = None
            print("\nNo users in voice channels")

class DiscordVoiceBot(commands.Bot):
    def __init__(self):
        intents = discord.Intents.default()
        intents.guilds = True
        intents.voice_states = True
        intents.members = True
        super().__init__(command_prefix='!', intents=intents)

    async def on_ready(self):
        global target_guild
        print(f'\nLogged in as {self.user} (ID: {self.user.id})')
        print(f'Intents enabled: guilds={self.intents.guilds}, voice_states={self.intents.voice_states}, members={self.intents.members}')

        # Find target guild
        target_guild = self.get_guild(GUILD_ID)
        if not target_guild:
            print(f"ERROR: Could not find guild with ID {GUILD_ID}")
            await self.close()
            return

        print(f'Connected to server: {target_guild.name}')
        print(f'Bot has access to {len(target_guild.voice_channels)} voice channels')

        # Wait a moment for voice states to populate
        await asyncio.sleep(2)

        # Initial user list update
        await update_voice_users()

        # Start HID listener
        self.loop.create_task(hid_listener())

    async def on_voice_state_update(self, member, before, after):
        """Called when someone joins/leaves/moves voice channels"""
        await update_voice_users()

async def hid_listener():
    """Listen for HID commands from keyboard"""
    global current_user_index, selected_user, voice_users

    keyboard_path = find_keyboard()
    if not keyboard_path:
        print("ERROR: Could not find OffReno keyboard!")
        return

    try:
        keyboard = hid.device()
        keyboard.open_path(keyboard_path)
        print("Connected to OffReno keyboard\n")
    except Exception as e:
        print(f"ERROR: Could not open keyboard: {e}")
        return

    print("Listening for encoder commands...\n")

    # Send initial state
    if voice_users:
        send_user_to_oled(keyboard, voice_users[current_user_index]['name'],
                         current_user_index, len(voice_users))

    while True:
        try:
            # Read HID data with timeout
            data = keyboard.read(33, timeout_ms=100)

            if data and len(data) > 0 and data[0] == 0xF2:  # Discord command
                command = data[1]

                if command == 0x01:  # CW rotation - next user
                    if voice_users:
                        current_user_index = (current_user_index + 1) % len(voice_users)
                        selected_user = voice_users[current_user_index]
                        send_user_to_oled(keyboard, selected_user['name'],
                                        current_user_index, len(voice_users))

                elif command == 0x02:  # CCW rotation - previous user
                    if voice_users:
                        current_user_index = (current_user_index - 1) % len(voice_users)
                        selected_user = voice_users[current_user_index]
                        send_user_to_oled(keyboard, selected_user['name'],
                                        current_user_index, len(voice_users))

                elif command == 0x03:  # Button press - mute/unmute
                    if selected_user:
                        member = selected_user['member']
                        try:
                            # Toggle mute
                            new_mute_state = not member.voice.mute
                            await member.edit(mute=new_mute_state)
                            action = "Muted" if new_mute_state else "Unmuted"
                            print(f"{action}: {member.display_name}")

                            # Send mute status back to keyboard
                            data_response = [0] * 33  # 33 bytes: 1 for report ID + 32 for data
                            data_response[0] = 0x00  # Report ID (required for HID)
                            data_response[1] = 0xF2  # Discord command identifier
                            data_response[2] = 0x04  # Mute status update
                            data_response[3] = 1 if new_mute_state else 0  # 1 = muted, 0 = unmuted
                            keyboard.write(bytes(data_response))
                        except discord.Forbidden:
                            print(f"ERROR: No permission to mute {member.display_name}")
                        except Exception as e:
                            print(f"ERROR muting user: {e}")

            await asyncio.sleep(0.01)  # Small delay to prevent CPU spinning

        except Exception as e:
            print(f"Error in HID listener: {e}")
            await asyncio.sleep(1)

    keyboard.close()

async def main():
    global bot, GUILD_ID

    print("Discord Voice Control starting...")
    print("Loading configuration...")

    BOT_TOKEN, GUILD_ID = load_config()

    bot = DiscordVoiceBot()

    try:
        await bot.start(BOT_TOKEN)
    except KeyboardInterrupt:
        print("\n\nStopping Discord Voice Control...")
        await bot.close()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\nDiscord Voice Control stopped")
