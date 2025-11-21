// src/echo_client.cpp
#include <iostream>
#include <string>
#include "protocol/protocol.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int get_session_id() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

int main() {
    int session_id = get_session_id();
    std::cout << "Starting Echo Client. Session ID: " << session_id << std::endl;
    
    while (true) {
        std::cout << "\nEnter command (start/letter/quit): ";
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "quit") {
            break;
        }
        
        // Очищаем файл перед отправкой нового сообщения
        Protocol::clear_messages();
        
        bool success = false;
        
        if (input == "start") {
            std::cout << "Sending PING start..." << std::endl;
            success = Protocol::send_ping_start(session_id);
        } else if (input.length() == 1) {
            std::cout << "Sending PING with letter: " << input[0] << std::endl;
            success = Protocol::send_ping_guess(session_id, input[0]);
        } else {
            std::cout << "Invalid input. Use 'start', single letter, or 'quit'" << std::endl;
            continue;
        }
        
        if (!success) {
            std::cout << "Failed to send message" << std::endl;
            continue;
        }
        
        std::cout << "Waiting for PONG response..." << std::endl;
        
        // Ждем PONG ответ
        auto response = Protocol::wait_for_pong(session_id, 5000);
        
        if (!response.message_type.empty()) {
            std::cout << "Received PONG!" << std::endl;
            std::cout << "  Game state: " << response.game_state << std::endl;
            std::cout << "  Errors left: " << response.errors_left << std::endl;
            std::cout << "  Status: " << response.status << std::endl;
            std::cout << "  Additional: " << response.additional_info << std::endl;
            
            Protocol::clear_messages();// ОЧИЩАЕМ файл после успешного получения PONG
            // Это подтверждает что мы получили ответ и готовы к следующему сообщению
        } else {
            std::cout << "No PONG response received (timeout)" << std::endl;
            // При таймауте тоже очищаем, чтобы не копился мусор
            
        }
    }
    
    return 0;
}