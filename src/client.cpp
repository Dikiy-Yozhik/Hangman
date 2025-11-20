#include <iostream>
#include <string>
#include "ipc/file_socket.cpp"

int main() {
    std::cout << "Starting Echo Client..." << std::endl;
    
    while (true) {
        std::cout << "Enter message (or 'quit'): ";
        std::string message;
        std::getline(std::cin, message);
        
        if (message == "quit") break;
        
        FileSocket::clear_messages(IPC::SOCKET_FILE);
        
        if (FileSocket::write_message(IPC::SOCKET_FILE, message)) {
            std::cout << "Sent: " << message << std::endl;
        } else {
            std::cout << "Failed to send message" << std::endl;
            continue;
        }
        
        bool response_received = false;
        auto start = std::chrono::steady_clock::now();
        
        while (!response_received) {
            auto responses = FileSocket::read_messages(IPC::SOCKET_FILE);
            
            for (const auto& response : responses) {
                if (response.find("ECHO: " + message) == 0) {
                    std::cout << "Received: " << response << std::endl;
                    response_received = true;
                    break;
                }
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (elapsed > 3000) { 
                std::cout << "Timeout waiting for response" << std::endl;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    return 0;
}