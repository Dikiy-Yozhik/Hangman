#include "protocol.hpp"
#include "../ipc/file_socket.hpp"
#include "../ipc/ipc_common.hpp"
#include <sstream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace Protocol {

// ==================== Вспомогательные функции ====================

uint32_t calculate_checksum(const MessageHeader& header, const std::vector<uint8_t>& payload) {
    // Простая XOR checksum для демонстрации
    uint32_t checksum = 0;
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);
    
    for (size_t i = 0; i < sizeof(MessageHeader) - sizeof(header.checksum); ++i) {
        checksum ^= header_bytes[i];
    }
    
    for (uint8_t byte : payload) {
        checksum ^= byte;
    }
    
    return checksum;
}

bool validate_message(const BinaryMessage& message) {
    if (message.header.session_id == 0) return false;
    if (message.header.message_type != MessageType::PING && 
        message.header.message_type != MessageType::PONG) return false;
    if (message.header.payload_size != message.payload.size()) return false;
    
    // Проверяем checksum
    uint32_t calculated_checksum = calculate_checksum(message.header, message.payload);
    return calculated_checksum == message.header.checksum;
}

// ==================== Сериализация/десериализация ====================

std::vector<uint8_t> serialize_game_state(const GameState& state) {
    std::vector<uint8_t> data;
    
    // Добавляем тип payload
    data.push_back(PayloadType::GAME_STATE);
    
    // Сериализуем display_word (длина + строка)
    uint16_t word_length = static_cast<uint16_t>(state.display_word.length());
    data.push_back(static_cast<uint8_t>(word_length >> 8));
    data.push_back(static_cast<uint8_t>(word_length & 0xFF));
    data.insert(data.end(), state.display_word.begin(), state.display_word.end());
    
    // Сериализуем errors_left
    data.push_back(state.errors_left);
    
    // Сериализуем status
    data.push_back(state.status);
    
    // Сериализуем additional_info (длина + строка)
    uint16_t info_length = static_cast<uint16_t>(state.additional_info.length());
    data.push_back(static_cast<uint8_t>(info_length >> 8));
    data.push_back(static_cast<uint8_t>(info_length & 0xFF));
    data.insert(data.end(), state.additional_info.begin(), state.additional_info.end());
    
    return data;
}

GameState deserialize_game_state(const std::vector<uint8_t>& data) {
    GameState state;
    size_t offset = 0;
    
    if (data.empty() || data[offset++] != PayloadType::GAME_STATE) {
        return state;
    }
    
    // Десериализуем display_word
    if (offset + 2 <= data.size()) {
        uint16_t word_length = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + word_length <= data.size()) {
            state.display_word.assign(data.begin() + offset, data.begin() + offset + word_length);
            offset += word_length;
        }
    }
    
    // Десериализуем errors_left
    if (offset < data.size()) {
        state.errors_left = data[offset++];
    }
    
    // Десериализуем status
    if (offset < data.size()) {
        state.status = data[offset++];
    }
    
    // Десериализуем additional_info
    if (offset + 2 <= data.size()) {
        uint16_t info_length = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + info_length <= data.size()) {
            state.additional_info.assign(data.begin() + offset, data.begin() + offset + info_length);
        }
    }
    
    return state;
}

std::string parse_ping_payload(const std::vector<uint8_t>& payload) {
    if (payload.empty()) return "";
    
    if (payload[0] == PayloadType::GAME_START) {
        return "start";
    } else if (payload[0] == PayloadType::LETTER_GUESS && payload.size() >= 2) {
        return std::string(1, static_cast<char>(payload[1]));
    }
    
    return "";
}

// ==================== Создание бинарных сообщений ====================

BinaryMessage create_ping_message(uint32_t session_id, uint32_t sequence, const std::string& payload) {
    BinaryMessage message;
    message.header.session_id = session_id;
    message.header.sequence = sequence;
    message.header.message_type = MessageType::PING;
    
    // Сериализуем payload
    if (payload == "start") {
        message.payload = {PayloadType::GAME_START};
    } else if (payload.length() == 1 && std::isalpha(payload[0])) {
        message.payload = {PayloadType::LETTER_GUESS, static_cast<uint8_t>(payload[0])};
    }
    
    message.header.payload_size = static_cast<uint32_t>(message.payload.size());
    message.header.checksum = calculate_checksum(message.header, message.payload);
    
    return message;
}

BinaryMessage create_pong_message(uint32_t session_id, uint32_t sequence, const GameState& game_state) {
    BinaryMessage message;
    message.header.session_id = session_id;
    message.header.sequence = sequence;
    message.header.message_type = MessageType::PONG;
    message.payload = serialize_game_state(game_state);
    message.header.payload_size = static_cast<uint32_t>(message.payload.size());
    message.header.checksum = calculate_checksum(message.header, message.payload);
    
    return message;
}

GameState parse_pong_payload(const std::vector<uint8_t>& payload) {
    return deserialize_game_state(payload);
}

// ==================== Отправка/получение бинарных сообщений ====================

bool send_binary_ping(uint32_t session_id, uint32_t sequence, const std::string& payload) {
    BinaryMessage message = create_ping_message(session_id, sequence, payload);
    
    std::vector<uint8_t> serialized_data;
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&message.header);
    serialized_data.insert(serialized_data.end(), header_bytes, header_bytes + sizeof(MessageHeader));
    serialized_data.insert(serialized_data.end(), message.payload.begin(), message.payload.end());
    
    // Конвертируем vector<uint8_t> в vector<char>
    std::vector<char> char_data(serialized_data.begin(), serialized_data.end());
    return FileSocket::write_to_client_region(session_id, char_data);
}

bool send_binary_pong(uint32_t session_id, uint32_t sequence, const GameState& game_state) {
    BinaryMessage message = create_pong_message(session_id, sequence, game_state);
    
    std::vector<uint8_t> serialized_data;
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&message.header);
    serialized_data.insert(serialized_data.end(), header_bytes, header_bytes + sizeof(MessageHeader));
    serialized_data.insert(serialized_data.end(), message.payload.begin(), message.payload.end());
    
    std::vector<char> char_data(serialized_data.begin(), serialized_data.end());
    return FileSocket::write_to_server_region(session_id, char_data);
}

BinaryMessage receive_binary_message(uint32_t session_id, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        std::vector<char> char_data;
        BinaryMessage found_message;
        bool found_valid_message = false;
        uint32_t actual_session_id = session_id; // ← ДОБАВЛЯЕМ
        
        if (session_id == 0) {
            // СЕРВЕРНЫЙ РЕЖИМ: проверяем ВСЕ регионы
            for (uint32_t region_index = 1; region_index <= IPC::MAX_SESSIONS; region_index++) {
                std::cout << "DEBUG: Checking region " << region_index << std::endl;
                char_data = FileSocket::read_from_client_region(region_index);
                std::cout << "DEBUG: Region " << region_index << " data size: " << char_data.size() << std::endl;
                
                if (!char_data.empty() && char_data.size() >= sizeof(MessageHeader)) {
                    std::cout << "DEBUG: Found message in region " << region_index << std::endl;
                    
                    // Парсим сообщение
                    std::vector<uint8_t> data(char_data.begin(), char_data.end());
                    std::memcpy(&found_message.header, data.data(), sizeof(MessageHeader));
                    
                    std::cout << "DEBUG: Message session_id = " << found_message.header.session_id << std::endl;
                    
                    // ← ВАЖНО: Запоминаем настоящий session_id!
                    actual_session_id = found_message.header.session_id;
                    
                    if (data.size() >= sizeof(MessageHeader) + found_message.header.payload_size) {
                        found_message.payload.assign(data.begin() + sizeof(MessageHeader), 
                                                   data.begin() + sizeof(MessageHeader) + found_message.header.payload_size);
                        
                        // Проверяем валидность
                        if (validate_message(found_message)) {
                            std::cout << "DEBUG: Valid message found! Type: " << found_message.header.message_type << std::endl;
                            found_valid_message = true;
                            break;
                        }
                    }
                }
            }
        } else {
            // КЛИЕНТСКИЙ РЕЖИМ
            actual_session_id = session_id; // ← Для клиента оставляем как есть
            char_data = FileSocket::read_from_server_region(session_id);
            if (!char_data.empty() && char_data.size() >= sizeof(MessageHeader)) {
                std::vector<uint8_t> data(char_data.begin(), char_data.end());
                std::memcpy(&found_message.header, data.data(), sizeof(MessageHeader));
                
                if (data.size() >= sizeof(MessageHeader) + found_message.header.payload_size) {
                    found_message.payload.assign(data.begin() + sizeof(MessageHeader), 
                                               data.begin() + sizeof(MessageHeader) + found_message.header.payload_size);
                    
                    if (validate_message(found_message)) {
                        found_valid_message = true;
                    }
                }
            }
        }
        
        if (found_valid_message) {
            // ← ВАЖНО: Устанавливаем правильный session_id!
            found_message.header.session_id = actual_session_id;
            std::cout << "DEBUG: Returning valid message to caller, session_id=" 
                      << found_message.header.session_id << std::endl;
            return found_message;
        }
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeout_ms) {
            std::cout << "DEBUG: Timeout reached" << std::endl;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "DEBUG: No message found, returning empty" << std::endl;
    return BinaryMessage();
}

// ==================== Текстовые функции (обратная совместимость) ====================

bool send_ping_start(uint32_t session_id) {
    std::string message = "PING " + std::to_string(session_id) + " start";
    return FileSocket::write_message(IPC::SOCKET_FILE, message);
}

bool send_ping_guess(uint32_t session_id, char letter) {
    std::string message = "PING " + std::to_string(session_id) + " " + letter;
    return FileSocket::write_message(IPC::SOCKET_FILE, message);
}

bool send_pong(uint32_t session_id, const std::string& game_state, 
               int errors_left, const std::string& status,
               const std::string& additional_info) {
    std::string message = "PONG " + std::to_string(session_id) + " " +
                         game_state + " " + std::to_string(errors_left) + " " + status;
    
    if (!additional_info.empty()) {
        message += " " + additional_info;
    }
    
    return FileSocket::write_message(IPC::SOCKET_FILE, message);
}

ParsedMessage read_next_message(int timeout_ms) {
    ParsedMessage result;
    auto messages = FileSocket::wait_for_message(IPC::SOCKET_FILE, timeout_ms);
    
    if (messages.empty()) return result;
    
    return parse_message_from_string(messages[0]);
}

ParsedMessage wait_for_pong(uint32_t session_id, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        auto messages = FileSocket::read_messages(IPC::SOCKET_FILE);
        
        for (const auto& message : messages) {
            ParsedMessage parsed = parse_message_from_string(message);
            if (parsed.message_type == "PONG" && parsed.session_id == session_id) {
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

// ==================== Вспомогательные функции ====================

void clear_messages() {
    FileSocket::clear_messages(IPC::SOCKET_FILE);
}

bool is_ping_message(const ParsedMessage& msg) {
    return msg.message_type == "PING";
}

bool is_pong_message(const ParsedMessage& msg) {
    return msg.message_type == "PONG";
}

bool validate_ping_payload(const std::string& payload) {
    if (payload.empty()) return false;
    if (payload == "start") return true;
    if (payload.length() == 1 && std::isalpha(payload[0])) return true;
    return false;
}

bool validate_session_id(uint32_t session_id) {
    return session_id != 0;
}

bool validate_game_state(const ParsedMessage& msg) {
    return !msg.game_state.empty() && 
           msg.errors_left >= 0 && msg.errors_left <= 6 &&
           !msg.status.empty();
}

// ==================== Конвертация между форматами ====================

ParsedMessage binary_to_parsed_message(const BinaryMessage& binary_msg) {
    ParsedMessage result;
    
    result.session_id = binary_msg.header.session_id;
    
    if (binary_msg.header.message_type == MessageType::PING) {
        result.message_type = "PING";
        result.payload = parse_ping_payload(binary_msg.payload);
    } else if (binary_msg.header.message_type == MessageType::PONG) {
        result.message_type = "PONG";
        GameState game_state = parse_pong_payload(binary_msg.payload);
        result.game_state = game_state.display_word;
        result.errors_left = game_state.errors_left;
        
        // Конвертируем статус обратно в строку
        switch (game_state.status) {
            case GameStatus::IN_PROGRESS: result.status = "IN_PROGRESS"; break;
            case GameStatus::WIN: result.status = "WIN"; break;
            case GameStatus::LOSE: result.status = "LOSE"; break;
            case GameStatus::ERROR_STATE: result.status = "ERROR"; break;
            default: result.status = "UNKNOWN";
        }
        
        result.additional_info = game_state.additional_info;
    }
    
    return result;
}

BinaryMessage parsed_to_binary_message(const ParsedMessage& parsed_msg) {
    // Эта функция требует знания sequence number, поэтому оставляем как заглушку
    (void)parsed_msg;
    return BinaryMessage();
}

// ==================== Внутренние функции ====================

ParsedMessage parse_message_from_string(const std::string& raw_message) {
    ParsedMessage result;
    std::istringstream iss(raw_message);
    
    std::string message_type;
    if (!(iss >> message_type >> result.session_id)) {
        return result;
    }
    
    result.message_type = message_type;
    
    if (message_type == "PING") {
        std::string remaining;
        std::getline(iss, remaining);
        
        size_t start = remaining.find_first_not_of(" \t");
        if (start != std::string::npos) {
            result.payload = remaining.substr(start);
        }
    }
    else if (message_type == "PONG") {
        std::string remaining;
        std::getline(iss, remaining);
        
        size_t start_pos = remaining.find_first_not_of(" \t");
        if (start_pos == std::string::npos) return result;
        remaining = remaining.substr(start_pos);
        
        std::vector<std::string> parts;
        std::istringstream part_stream(remaining);
        std::string part;
        while (part_stream >> part) {
            parts.push_back(part);
        }
        
        if (parts.size() >= 3) {
            result.game_state = parts[0];
            result.errors_left = std::stoi(parts[1]);
            result.status = parts[2];
            
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

} // namespace Protocol