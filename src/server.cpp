#include <iostream>
#include <string>
#include "ipc/file_socket.cpp"

int main() {
    std::cout << "Starting Echo Server..." << std::endl;
    
    FileSocket::clear_messages(IPC::SOCKET_FILE);
    
    while (true) {
        std::cout << "Waiting for messages..." << std::endl;
        
        auto messages = FileSocket::wait_for_message(IPC::SOCKET_FILE, 5000);
        
        if (!messages.empty()) {
            for (const auto& msg : messages) {
                if (msg.find("ECHO: ") != 0) {  
                    std::cout << "Received from client: " << msg << std::endl;
                    
                    std::string response = "ECHO: " + msg;
                    if (FileSocket::write_message(IPC::SOCKET_FILE, response)) {
                        std::cout << "Sent: " << response << std::endl;
                    }
                }
            }
            
            FileSocket::clear_messages(IPC::SOCKET_FILE);
            std::cout << "Cleared socket file" << std::endl;
        } else {
            std::cout << "No new messages" << std::endl;
        }
    }
    
    return 0;
}