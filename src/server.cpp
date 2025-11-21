// src/echo_server.cpp
#include <iostream>
#include <string>
#include "protocol/protocol.hpp"

int main() {
    std::cout << "Starting Echo Server with Protocol..." << std::endl;
    
    // Очищаем файл только при старте сервера
    Protocol::clear_messages();
    
    while (true) {
        std::cout << "Waiting for PING messages..." << std::endl;
        
        // Ждем любое сообщение
        auto message = Protocol::read_next_message(10000);
        
        if (!message.message_type.empty()) {
            std::cout << "Received: " << message.message_type 
                    << " from session " << message.session_id << std::endl;
            
            // Обрабатываем ТОЛЬКО PING сообщения
            if (message.message_type == Protocol::MessageType::PING) {
                std::cout << "PING payload: " << message.payload << std::endl;
                
                // Эхо-ответ
                if (message.payload == "start") {
                    Protocol::send_pong(message.session_id, 
                                    "******",
                                    6,
                                    Protocol::GameStatus::IN_PROGRESS,
                                    "Echo: Game started!");
                    std::cout << "Sent PONG for start" << std::endl;
                } else {
                    Protocol::send_pong(message.session_id,
                                    "******",  
                                    6,
                                    Protocol::GameStatus::IN_PROGRESS, 
                                    "Echo: You sent '" + message.payload + "'");
                    std::cout << "Sent PONG for letter: " << message.payload << std::endl;
                }
            } else {
                // Игнорируем PONG и другие сообщения
                std::cout << "Ignoring non-PING message" << std::endl;
            }
        }
    }
    
    return 0;
}