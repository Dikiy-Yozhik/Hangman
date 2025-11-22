#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace Protocol {

// ==================== Бинарные структуры протокола ====================

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t session_id;
    uint32_t sequence;
    uint32_t message_type;   // 1 = PING, 2 = PONG
    uint32_t payload_size;
    uint32_t checksum;
};
#pragma pack(pop)

// Константы для типов сообщений
namespace MessageType {
    const uint32_t PING = 1;
    const uint32_t PONG = 2;
}

namespace PayloadType {
    const uint8_t GAME_START = 1;
    const uint8_t LETTER_GUESS = 2;
    const uint8_t GAME_STATE = 3;
    const uint8_t GAME_RESULT = 4;
}

namespace GameStatus {
    const uint8_t IN_PROGRESS = 1;
    const uint8_t WIN = 2;
    const uint8_t LOSE = 3;
    const uint8_t ERROR_STATE = 4;
}

// Вспомогательные структуры для бинарного протокола
struct BinaryMessage {
    MessageHeader header;
    std::vector<uint8_t> payload;
};

struct GameState {
    std::string display_word;
    uint8_t errors_left;
    uint8_t status;
    std::string additional_info;
};

// ==================== Бинарные функции протокола ====================

// Сериализация/десериализация
BinaryMessage create_ping_message(uint32_t session_id, uint32_t sequence, const std::string& payload);
BinaryMessage create_pong_message(uint32_t session_id, uint32_t sequence, const GameState& game_state);
GameState parse_pong_payload(const std::vector<uint8_t>& payload);
std::string parse_ping_payload(const std::vector<uint8_t>& payload);

// Отправка/получение бинарных сообщений
bool send_binary_ping(uint32_t session_id, uint32_t sequence, const std::string& payload);
bool send_binary_pong(uint32_t session_id, uint32_t sequence, const GameState& game_state);
BinaryMessage receive_binary_message(uint32_t session_id, int timeout_ms = 5000);

// Вспомогательные функции для бинарного протокола
uint32_t calculate_checksum(const MessageHeader& header, const std::vector<uint8_t>& payload);
bool validate_message(const BinaryMessage& message);
std::vector<uint8_t> serialize_game_state(const GameState& state);
GameState deserialize_game_state(const std::vector<uint8_t>& data);

// ==================== Текстовые функции (для обратной совместимости) ====================

// Старые текстовые структуры
struct ParsedMessage {
    std::string message_type;
    uint32_t session_id;
    std::string payload;
    std::string game_state;
    int errors_left;
    std::string status;
    std::string additional_info;
};

// Старые текстовые функции
bool send_ping_start(uint32_t session_id);
bool send_ping_guess(uint32_t session_id, char letter);
bool send_pong(uint32_t session_id, const std::string& game_state, int errors_left, 
               const std::string& status, const std::string& additional_info = "");

ParsedMessage read_next_message(int timeout_ms = 5000);
ParsedMessage wait_for_pong(uint32_t session_id, int timeout_ms = 5000);
ParsedMessage parse_message_from_string(const std::string& raw_message);

void clear_messages();
bool is_ping_message(const ParsedMessage& msg);
bool is_pong_message(const ParsedMessage& msg);

// Валидация
bool validate_ping_payload(const std::string& payload);
bool validate_session_id(uint32_t session_id);
bool validate_game_state(const ParsedMessage& msg);

// Конвертация между форматами
ParsedMessage binary_to_parsed_message(const BinaryMessage& binary_msg);
BinaryMessage parsed_to_binary_message(const ParsedMessage& parsed_msg);

} // namespace Protocol

#endif // PROTOCOL_HPP