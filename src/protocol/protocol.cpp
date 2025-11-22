#include "protocol.hpp"
#include "../ipc/file_socket.hpp"
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace Protocol {

// ==================== Вспомогательные функции ====================

uint32_t calculate_checksum(const MessageHeader& header, const std::vector<uint8_t>& payload) {
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
    
    uint32_t calculated_checksum = calculate_checksum(message.header, message.payload);
    return calculated_checksum == message.header.checksum;
}

// ==================== Сериализация/десериализация ====================

std::vector<uint8_t> serialize_game_state(const GameState& state) {
    std::vector<uint8_t> data;
    
    data.push_back(PayloadType::GAME_STATE);
    
    uint16_t word_length = static_cast<uint16_t>(state.display_word.length());
    data.push_back(static_cast<uint8_t>(word_length >> 8));
    data.push_back(static_cast<uint8_t>(word_length & 0xFF));
    data.insert(data.end(), state.display_word.begin(), state.display_word.end());
    
    data.push_back(state.errors_left);
    data.push_back(state.status);
    
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
    
    if (offset + 2 <= data.size()) {
        uint16_t word_length = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
        
        if (offset + word_length <= data.size()) {
            state.display_word.assign(data.begin() + offset, data.begin() + offset + word_length);
            offset += word_length;
        }
    }
    
    if (offset < data.size()) {
        state.errors_left = data[offset++];
    }
    
    if (offset < data.size()) {
        state.status = data[offset++];
    }
    
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

// ==================== Создание сообщений ====================

BinaryMessage create_ping_message(uint32_t session_id, uint32_t sequence, const std::string& payload) {
    BinaryMessage message;
    message.header.session_id = session_id;
    message.header.sequence = sequence;
    message.header.message_type = MessageType::PING;
    
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

// ==================== Основные функции протокола ====================

bool send_binary_ping(uint32_t session_id, uint32_t sequence, const std::string& payload) {
    BinaryMessage message = create_ping_message(session_id, sequence, payload);
    
    std::vector<uint8_t> serialized_data;
    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&message.header);
    serialized_data.insert(serialized_data.end(), header_bytes, header_bytes + sizeof(MessageHeader));
    serialized_data.insert(serialized_data.end(), message.payload.begin(), message.payload.end());
    
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
        uint32_t actual_session_id = session_id;
        
        if (session_id == 0) {
            for (uint32_t region_index = 1; region_index <= IPC::MAX_SESSIONS; region_index++) {
                char_data = FileSocket::read_from_client_region(region_index);
                
                if (!char_data.empty() && char_data.size() >= sizeof(MessageHeader)) {
                    std::vector<uint8_t> data(char_data.begin(), char_data.end());
                    std::memcpy(&found_message.header, data.data(), sizeof(MessageHeader));
                    
                    actual_session_id = found_message.header.session_id;
                    
                    if (data.size() >= sizeof(MessageHeader) + found_message.header.payload_size) {
                        found_message.payload.assign(data.begin() + sizeof(MessageHeader), 
                                                   data.begin() + sizeof(MessageHeader) + found_message.header.payload_size);
                        
                        if (validate_message(found_message)) {
                            found_valid_message = true;
                            break;
                        }
                    }
                }
            }
        } else {
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
            found_message.header.session_id = actual_session_id;
            return found_message;
        }
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeout_ms) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return BinaryMessage();
}

// ==================== Валидация ====================

GameState parse_pong_payload(const std::vector<uint8_t>& payload) {
    return deserialize_game_state(payload);
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

} 
