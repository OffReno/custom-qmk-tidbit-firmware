#!/usr/bin/env python3
"""
Volume Balancer for QMK TIDBIT Keyboard
Controls Discord and Game volumes via HID RAW commands from encoder 1

Production mode: Encoder 1 controls Discord/Game volume balance
  CW rotation  = Game up, Discord down
  CCW rotation = Discord up, Game down
  Button press = Balance (Master 100%, both 75%)

Testing mode: Run with --test flag to use keyboard numeric keys
  1 key = Balance (Master 100%, both 75%)
  2 key = Discord up, Game down (CCW)
  3 key = Game up, Discord down (CW)

Game detection: Edit games.txt to add/remove games
"""

import time
import sys
import os

# Check for test mode
TEST_MODE = "--test" in sys.argv

try:
    import hid
    HID_AVAILABLE = True
except ImportError:
    HID_AVAILABLE = False
    if not TEST_MODE:
        print("ERROR: hidapi not available - install with: pip install hidapi")
        exit(1)

try:
    from pycaw.pycaw import AudioUtilities, ISimpleAudioVolume, IAudioEndpointVolume
    from comtypes import CLSCTX_ALL
    PYCAW_AVAILABLE = True
except ImportError:
    PYCAW_AVAILABLE = False
    print("ERROR: pycaw not available - install with: pip install pycaw comtypes")
    exit(1)

# Keyboard library for testing mode
if TEST_MODE:
    try:
        import keyboard
        KEYBOARD_AVAILABLE = True
    except ImportError:
        KEYBOARD_AVAILABLE = False
        print("ERROR: keyboard library not available - install with: pip install keyboard")
        exit(1)

# QMK HID RAW settings
VID = 0x6E61  # OffReno keyboard vendor ID
PID = 0x6064  # OffReno keyboard product ID

VOLUME_STEP = 0.06  # 6% volume change per encoder click
STEAM_GAMES_PATH = r"D:\Steam\steamapps\common"
GAMES_FILE = os.path.join(os.path.dirname(__file__), "games.txt")

def load_game_names():
    """Load game names from games.txt file"""
    game_names = []
    if os.path.exists(GAMES_FILE):
        try:
            with open(GAMES_FILE, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    # Skip comments and empty lines
                    if line and not line.startswith('#'):
                        game_names.append(line.lower())
        except Exception as e:
            print(f"Warning: Could not read games.txt: {e}")

    # Add Discord as fallback
    if 'discord' not in game_names:
        game_names.append('discord')

    return game_names

def scan_steam_games():
    """Scan Steam directory and update games.txt"""
    print(f"\nScanning Steam games directory: {STEAM_GAMES_PATH}")

    if not os.path.exists(STEAM_GAMES_PATH):
        print(f"Warning: Steam directory not found: {STEAM_GAMES_PATH}")
        return

    try:
        game_folders = os.listdir(STEAM_GAMES_PATH)
        game_names = []

        for folder in game_folders:
            # Convert folder name to lowercase and extract key words
            name = folder.lower().replace(' ', '').replace('-', '').replace('_', '')
            # Take first significant word (at least 4 chars)
            if len(name) >= 4:
                game_names.append(name[:15])  # Limit length

        # Write to games.txt
        with open(GAMES_FILE, 'w', encoding='utf-8') as f:
            f.write("# Steam Games List - Edit this file to add/remove games\n")
            f.write("# Each line should contain part of the game executable name (lowercase, no .exe)\n")
            f.write("# The script will match these against running audio sessions\n\n")
            f.write("# Your Steam Games (auto-detected)\n")
            for name in sorted(set(game_names)):
                f.write(f"{name}\n")
            f.write("\n# Other apps\ndiscord\nchrome\nspotify\n")

        print(f"Updated games.txt with {len(game_names)} games")
    except Exception as e:
        print(f"Error scanning Steam directory: {e}")

def get_master_volume():
    """Get system master volume (0.0 to 1.0)"""
    try:
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        volume = interface.QueryInterface(IAudioEndpointVolume)
        return volume.GetMasterVolumeLevelScalar()
    except Exception as e:
        print(f"Error getting master volume: {e}")
        return 0.0

def set_master_volume(volume):
    """Set system master volume (0.0 to 1.0)"""
    try:
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        volume_obj = interface.QueryInterface(IAudioEndpointVolume)
        volume_obj.SetMasterVolumeLevelScalar(max(0.0, min(1.0, volume)), None)
    except Exception as e:
        print(f"Error setting master volume: {e}")

def list_all_audio_sessions():
    """List all active audio sessions for debugging"""
    print("\n=== Active Audio Sessions ===")
    sessions = AudioUtilities.GetAllSessions()
    found_any = False
    for session in sessions:
        if session.Process and session.Process.name():
            volume = session._ctl.QueryInterface(ISimpleAudioVolume).GetMasterVolume()
            print(f"  {session.Process.name()}: {int(volume*100)}%")
            found_any = True
    if not found_any:
        print("  No active audio sessions found")
    print("=============================\n")

def get_session_by_name(name_part):
    """Find audio session by process name (case-insensitive partial match)"""
    sessions = AudioUtilities.GetAllSessions()
    for session in sessions:
        if session.Process and session.Process.name():
            process_name = session.Process.name().lower()
            if name_part.lower() in process_name:
                return session
    return None

def set_app_volume(session, volume):
    """Set application volume (0.0 to 1.0)"""
    if session:
        volume_interface = session._ctl.QueryInterface(ISimpleAudioVolume)
        volume_interface.SetMasterVolume(max(0.0, min(1.0, volume)), None)

def get_app_volume(session):
    """Get application volume (0.0 to 1.0)"""
    if session:
        volume_interface = session._ctl.QueryInterface(ISimpleAudioVolume)
        return volume_interface.GetMasterVolume()
    return 0.0

def balance_volumes(discord_session, game_session):
    """Rebalance: Master volume to 100%, both apps to 75%"""
    if not discord_session:
        print("\nCannot balance: Discord session not found!")
        return
    if not game_session:
        print("\nCannot balance: Game session not found!")
        return

    print(f"\nBalancing volumes...")
    print(f"  Before: Master={int(get_master_volume()*100)}% Discord={int(get_app_volume(discord_session)*100)}% Game={int(get_app_volume(game_session)*100)}%")

    # Set master volume to 100%
    set_master_volume(1.0)
    print(f"  Set master to 100%")

    # Set both apps to 75%
    set_app_volume(discord_session, 0.75)
    print(f"  Set Discord to 75%")

    set_app_volume(game_session, 0.75)
    print(f"  Set Game to 75%")

    # Verify results
    master_vol = int(get_master_volume() * 100)
    discord_vol = int(get_app_volume(discord_session) * 100)
    game_vol = int(get_app_volume(game_session) * 100)
    print(f"  After: Master={master_vol}% Discord={discord_vol}% Game={game_vol}%")

def adjust_volumes(discord_session, game_session, direction):
    """
    Adjust volumes inversely by 6% each
    direction: 1 = game up/discord down, -1 = discord up/game down
    """
    if not discord_session:
        print("\nDiscord session not found!", end='\r')
        return
    if not game_session:
        print("\nGame session not found!", end='\r')
        return

    discord_vol = get_app_volume(discord_session)
    game_vol = get_app_volume(game_session)

    # Adjust volumes inversely by VOLUME_STEP (2%)
    new_discord = discord_vol - (direction * VOLUME_STEP)
    new_game = game_vol + (direction * VOLUME_STEP)

    set_app_volume(discord_session, new_discord)
    set_app_volume(game_session, new_game)

    master_vol = int(get_master_volume() * 100)
    print(f"Master: {master_vol}%  Discord: {int(new_discord*100)}%  Game: {int(new_game*100)}%", end='\r')

def find_keyboard():
    """Find the OffReno keyboard RAW HID interface"""
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            # QMK RAW HID uses usage_page 0xFF60 and usage 0x61
            # Check if this is the RAW HID interface
            if device.get('usage_page') == 0xFF60 and device.get('usage') == 0x61:
                print(f"Found RAW HID interface: {device.get('interface_number', 'N/A')}")
                return device['path']

    # Fallback: try any interface with matching VID/PID
    print("RAW HID interface not found by usage page, trying fallback...")
    for device in hid.enumerate():
        if device['vendor_id'] == VID and device['product_id'] == PID:
            print(f"Using interface: {device.get('interface_number', 'N/A')} usage_page={device.get('usage_page', 'N/A'):04X}")
            return device['path']

    return None

def main_test_mode():
    """Test mode - use keyboard keys"""
    print("Volume Balancer starting in TEST MODE...")
    print("\nControls:")
    print("  1 key = Balance (Master 100%, both 75%)")
    print("  2 key = Discord up, Game down (CCW)")
    print("  3 key = Game up, Discord down (CW)")
    print("  ESC   = Exit\n")

    # List all audio sessions at startup
    list_all_audio_sessions()

    # Load game names from games.txt
    GAME_NAMES = load_game_names()
    print(f"Loaded {len(GAME_NAMES)} game names from games.txt")
    print("TIP: Edit games.txt to add/remove games, or run with --scan to auto-detect Steam games\n")

    # Load sessions at startup
    print("Loading audio sessions...")

    # Find Discord session (always use "discord")
    discord_session = get_session_by_name("discord")
    if discord_session:
        print(f"Found Discord: {discord_session.Process.name()}")
    else:
        print("WARNING: Discord session not found!")

    # Find game session (exclude Discord from game search)
    game_session = None
    for game_name in GAME_NAMES:
        if game_name == "discord":  # Skip Discord in game search
            continue
        temp_session = get_session_by_name(game_name)
        if temp_session:
            # Make sure we didn't accidentally find Discord (e.g., "disco" matches "discord")
            if temp_session.Process and temp_session.Process.name().lower() != "discord.exe":
                game_session = temp_session
                print(f"Found game: {game_session.Process.name()}")
                break

    if not game_session:
        print("WARNING: Game session not found! Make sure a game is running with audio.")

    print()

    # Cache sessions (refresh every 5 seconds)
    last_refresh = time.time()

    print("Listening for keys... (Press ESC to stop)\n")

    # Flag to control the loop
    running = True

    # Define key press handlers
    def on_key_1(event):
        if event.event_type == 'down':
            balance_volumes(discord_session, game_session)

    def on_key_2(event):
        if event.event_type == 'down':
            adjust_volumes(discord_session, game_session, -1)  # Discord up, Game down (CCW)

    def on_key_3(event):
        if event.event_type == 'down':
            adjust_volumes(discord_session, game_session, 1)  # Game up, Discord down (CW)

    def on_esc(event):
        nonlocal running
        if event.event_type == 'down':
            print("\n\nVolume Balancer stopped")
            running = False

    # Register key handlers
    keyboard.hook_key('1', on_key_1)
    keyboard.hook_key('2', on_key_2)
    keyboard.hook_key('3', on_key_3)
    keyboard.hook_key('esc', on_esc)

    try:
        while running:
            # Refresh sessions periodically
            if time.time() - last_refresh > 5.0:
                discord_session = get_session_by_name("discord")

                # Try to find game session (exclude Discord)
                game_session = None
                for game_name in GAME_NAMES:
                    if game_name == "discord":  # Skip Discord in game search
                        continue
                    temp_session = get_session_by_name(game_name)
                    if temp_session:
                        # Make sure we didn't accidentally find Discord
                        if temp_session.Process and temp_session.Process.name().lower() != "discord.exe":
                            game_session = temp_session
                            break

                last_refresh = time.time()

            time.sleep(0.1)  # Sleep longer since we're using event handlers

    except KeyboardInterrupt:
        print("\n\nVolume Balancer stopped")
    finally:
        keyboard.unhook_all()
        print("Keyboard hooks removed")

def main_hid_mode():
    """Production mode - use HID RAW from QMK keyboard encoder"""
    print("Volume Balancer starting in HID MODE (Encoder Control)...")
    print("\nEncoder 1 Controls:")
    print("  CW rotation  = Game up, Discord down")
    print("  CCW rotation = Discord up, Game down")
    print("  Button press = Balance (Master 100%, both 75%)\n")

    # List all audio sessions at startup
    list_all_audio_sessions()

    # Load game names from games.txt
    GAME_NAMES = load_game_names()
    print(f"Loaded {len(GAME_NAMES)} game names from games.txt")
    print("TIP: Edit games.txt to add/remove games\n")

    keyboard_path = find_keyboard()
    if not keyboard_path:
        print("ERROR: Could not find OffReno keyboard!")
        print(f"Looking for VID: 0x{VID:04X}, PID: 0x{PID:04X}")
        return

    try:
        keyboard = hid.device()
        keyboard.open_path(keyboard_path)
        print("Connected to OffReno keyboard")
    except Exception as e:
        print(f"ERROR: Could not open keyboard: {e}")
        return

    print("Listening for encoder commands... (Ctrl+C to stop)\n")

    # Load sessions at startup
    # Find Discord session (always use "discord")
    discord_session = get_session_by_name("discord")
    if discord_session:
        print(f"Found Discord: {discord_session.Process.name()}")
    else:
        print("WARNING: Discord session not found!")

    # Find game session (exclude Discord from game search)
    game_session = None
    for game_name in GAME_NAMES:
        if game_name == "discord":  # Skip Discord in game search
            continue
        temp_session = get_session_by_name(game_name)
        if temp_session:
            # Make sure we didn't accidentally find Discord (e.g., "disco" matches "discord")
            if temp_session.Process and temp_session.Process.name().lower() != "discord.exe":
                game_session = temp_session
                print(f"Found game: {game_session.Process.name()}")
                break

    if not game_session:
        print("WARNING: Game session not found! Make sure a game is running with audio.")

    # Cache sessions (refresh every 5 seconds)
    last_refresh = time.time()

    while True:
        try:
            # Refresh sessions periodically
            if time.time() - last_refresh > 5.0:
                discord_session = get_session_by_name("discord")

                # Try to find game session (exclude Discord)
                game_session = None
                for game_name in GAME_NAMES:
                    if game_name == "discord":  # Skip Discord in game search
                        continue
                    temp_session = get_session_by_name(game_name)
                    if temp_session:
                        # Make sure we didn't accidentally find Discord
                        if temp_session.Process and temp_session.Process.name().lower() != "discord.exe":
                            game_session = temp_session
                            break

                last_refresh = time.time()

            # Read HID data (blocking with 100ms timeout)
            data = keyboard.read(33, timeout_ms=100)

            if data and len(data) > 0:
                # Check for volume control commands
                if data[0] == 0xF1:  # Volume command identifier
                    command = data[1]

                    if command == 0x01:  # Clockwise: Game up, Discord down
                        adjust_volumes(discord_session, game_session, 1)
                    elif command == 0x02:  # Counter-clockwise: Discord up, Game down
                        adjust_volumes(discord_session, game_session, -1)
                    elif command == 0x03:  # Button press: Rebalance
                        balance_volumes(discord_session, game_session)

        except KeyboardInterrupt:
            print("\n\nVolume Balancer stopped")
            break
        except Exception as e:
            print(f"\nError: {e}")
            time.sleep(1)

    keyboard.close()

def main():
    # Check for --scan flag to update games.txt
    if "--scan" in sys.argv:
        print("Scanning Steam directory for games...")
        scan_steam_games()
        print("\nDone! Edit games.txt to customize the game list.")
        return

    if TEST_MODE:
        main_test_mode()
    else:
        main_hid_mode()

if __name__ == "__main__":
    main()
