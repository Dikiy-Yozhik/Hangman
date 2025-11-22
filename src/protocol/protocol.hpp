#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace Protocol {

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t session_id;
    uint32_t sequence;
    uint32_t message_type;
    uint32_t payload_size;
    uint32_t checksum;
};
#pragma pack(pop)

namespace MessageType {
    const uint32_t PING = 1;
    const uint32_t PONG = 2;
}

namespace PayloadType {
    const uint8_t GAME_START = 1;
    const uint8_t LETTER_GUESS = 2;
    const uint8_t GAME_STATE = 3;
}

namespace GameStatus {
    const uint8_t IN_PROGRESS = 1;
    const uint8_t WIN = 2;
    const uint8_t LOSE = 3;
    const uint8_t ERROR_STATE = 4;
}

struct GameState {
    std::string display_word;
    uint8_t errors_left;
    uint8_t status;
    std::string additional_info;
};

struct BinaryMessage {
    MessageHeader header;
    std::vector<uint8_t> payload;
};

// Основные функции протокола
bool send_binary_ping(uint32_t session_id, uint32_t sequence, const std::string& payload);
bool send_binary_pong(uint32_t session_id, uint32_t sequence, const GameState& game_state);
BinaryMessage receive_binary_message(uint32_t session_id, int timeout_ms = 5000);

// Вспомогательные функции
GameState parse_pong_payload(const std::vector<uint8_t>& payload);
std::string parse_ping_payload(const std::vector<uint8_t>& payload);
bool validate_ping_payload(const std::string& payload);
bool validate_session_id(uint32_t session_id);

} 

#endif