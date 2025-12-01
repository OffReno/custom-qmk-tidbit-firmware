# GitHub Setup Instructions

Follow these steps to push your custom TIDBIT firmware to GitHub.

## Step 1: Create New GitHub Repository

1. Go to https://github.com/OffReno
2. Click the **"+"** button in the top right
3. Click **"New repository"**
4. Fill in:
   - **Repository name:** `custom-qmk-tidbit-firmware`
   - **Description:** "Custom QMK firmware for TIDBIT numpad with system monitoring, volume balancing, and Discord voice control"
   - **Visibility:** Public (or Private if you prefer)
   - **DO NOT** check "Initialize with README" (we already have one)
5. Click **"Create repository"**

## Step 2: Initialize Git Repository (if not already done)

Open Git Bash or Terminal in the tidbit directory:

```bash
cd C:\Users\Renobatio\qmk_firmware\keyboards\nullbitsco\tidbit
```

Initialize git (if not already initialized):
```bash
git init
```

## Step 3: Add Files to Git

```bash
# Add all files
git add .

# Check what will be committed
git status
```

Make sure `discord_config.txt` is **NOT** in the list (it should be ignored by .gitignore).

## Step 4: Create Initial Commit

```bash
git commit -m "Initial commit: Custom TIDBIT firmware with monitoring, volume balance, and Discord control"
```

## Step 5: Link to GitHub Repository

Replace `OffReno` with your actual GitHub username if different:

```bash
git remote add origin https://github.com/OffReno/custom-qmk-tidbit-firmware.git
```

## Step 6: Push to GitHub

```bash
# Push to main branch
git push -u origin main
```

Or if your default branch is `master`:
```bash
git branch -M main
git push -u origin main
```

## Step 7: Verify Upload

1. Go to https://github.com/OffReno/custom-qmk-tidbit-firmware
2. You should see:
   - ✅ README.md displayed on the main page
   - ✅ All your firmware files
   - ✅ Python scripts
   - ✅ Batch files
   - ❌ **NO** discord_config.txt (it's in .gitignore)
   - ❌ **NO** .venv folder (it's in .gitignore)

## Step 8: Add Photos (Optional but Recommended)

1. Take photos of your TIDBIT setup
2. Upload to an image hosting service (Imgur, GitHub issues, etc.)
3. Edit README.md and replace the placeholder image URL:
   ```markdown
   ![TIDBIT Keyboard](https://i.imgur.com/your-actual-image.jpg)
   ```
4. Commit and push:
   ```bash
   git add README.md
   git commit -m "Add TIDBIT setup photo"
   git push
   ```

## Future Updates

When you make changes to the firmware:

```bash
# Stage your changes
git add .

# Commit with a descriptive message
git commit -m "Add feature X"
# or
git commit -m "Fix bug in Discord control"

# Push to GitHub
git push
```

## Common Git Commands

```bash
# Check status of files
git status

# See what changed
git diff

# View commit history
git log --oneline

# Undo uncommitted changes to a file
git checkout -- filename

# Pull latest changes (if working from multiple PCs)
git pull
```

## Security Note

**NEVER commit discord_config.txt to GitHub!**

If you accidentally commit it:
1. Remove it from git history
2. **IMMEDIATELY** regenerate your Discord bot token in the Developer Portal
3. The old token is now compromised

## Setting Up on a New PC

1. Clone the repository:
   ```bash
   git clone https://github.com/OffReno/custom-qmk-tidbit-firmware.git
   cd custom-qmk-tidbit-firmware
   ```

2. Follow the installation instructions in [README.md](README.md)

3. Create your `discord_config.txt` from the template

4. Update all absolute paths in .bat files and keymap.c

5. Compile and flash!

---

Need help? Open an issue on GitHub!
