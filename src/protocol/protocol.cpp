// src/protocol/protocol.cpp
#include "protocol.hpp"
#include "../ipc/file_socket.hpp" 
#include "../ipc/ipc_common.hpp" 
#include <sstream>
#include <string>
#include <chrono>
#include <thread>

namespace Protocol {

// ================================ Запись =================================================
bool send_ping_start(int session_id) {
    std::string message = MessageType::PING + " " + std::to_string(session_id) + " start";
    return FileSocket::write_message(IPC::SOCKET_FILE, message);
}

bool send_ping_guess(int session_id, char letter) {
    std::string message = MessageType::PING + " " + std::to_string(session_id) + " " + letter;
    return FileSocket::write_message(IPC::SOCKET_FILE, message);
}

bool send_pong(int session_id, const std::string& game_state, 
               int errors_left, const std::string& status,
               const std::string& additional_info) {
    std::string message = MessageType::PONG + " " + std::to_string(session_id) + " " +
                         game_state + " " + std::to_string(errors_left) + " " + status;
    
    if (!additional_info.empty()) {
        message += " " + additional_info;
    }
    
    return FileSocket::write_message(IPC::SOCKET_FILE, message);
}

// ===================================== Чтение ==============================================
ParsedMessage read_next_message(int timeout_ms) {
    ParsedMessage result;
    auto messages = FileSocket::wait_for_message(IPC::SOCKET_FILE, timeout_ms);
    
    if (messages.empty()) return result;
    
    return parse_message_from_string(messages[0]);
}

ParsedMessage wait_for_pong(int session_id, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        auto messages = FileSocket::read_messages(IPC::SOCKET_FILE);
        
        for (const auto& message : messages) {
            ParsedMessage parsed = parse_message_from_string(message);
            if (parsed.message_type == MessageType::PONG && 
                parsed.session_id == session_id) {
                return parsed;
            }
        }
        
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count() > timeout_ms) {
            return ParsedMessage();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Внутренняя функция - НЕ добавляем в header!
ParsedMessage parse_message_from_string(const std::string& raw_message) {
    ParsedMessage result;
    std::istringstream iss(raw_message);
    
    // Парсим базовые поля для всех сообщений
    if (!(iss >> result.message_type >> result.session_id)) {
        return result;
    }
    
    if (result.message_type == MessageType::PING) {
        // Для PING - просто читаем всю оставшуюся строку
        std::string remaining;
        std::getline(iss, remaining);
        
        size_t start = remaining.find_first_not_of(" \t");
        if (start != std::string::npos) {
            result.payload = remaining.substr(start);
        }
    }
    else if (result.message_type == MessageType::PONG) {
        // Для PONG - теперь простая структура: "______ 6 IN_PROGRESS Echo: Game started!"
        std::string remaining;
        std::getline(iss, remaining);
        
        size_t start_pos = remaining.find_first_not_of(" \t");
        if (start_pos == std::string::npos) return result;
        remaining = remaining.substr(start_pos);
        
        // Разбиваем на части
        std::vector<std::string> parts;
        std::istringstream part_stream(remaining);
        std::string part;
        while (part_stream >> part) {
            parts.push_back(part);
        }
        
        // parts[0] = "______" (game_state)
        // parts[1] = "6" (errors_left) 
        // parts[2] = "IN_PROGRESS" (status)
        // parts[3..] = additional_info
        
        if (parts.size() >= 3) {
            result.game_state = parts[0];
            result.errors_left = std::stoi(parts[1]);
            result.status = parts[2];
            
            // Additional info
            if (parts.size() > 3) {
                std::string additional;
                for (size_t i = 3; i < parts.size(); i++) {
                    if (!additional.empty()) additional += " ";
                    additional += parts[i];
                }
                result.additional_info = additional;
            }
        }
    }
    
    return result;
}

// ========================= Вспомогательные функции ============================
void clear_messages() {
    FileSocket::clear_messages(IPC::SOCKET_FILE);
}

bool is_ping_message(const ParsedMessage& msg) {
    return msg.message_type == MessageType::PING;
}

bool is_pong_message(const ParsedMessage& msg) {
    return msg.message_type == MessageType::PONG;
}

} 
