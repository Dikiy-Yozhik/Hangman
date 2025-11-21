#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>

namespace Protocol {

namespace MessageType {
    const std::string PING = "PING";
    const std::string PONG = "PONG";
}

namespace GameStatus {
    const std::string IN_PROGRESS = "IN_PROGRESS";
    const std::string WIN = "WIN";
    const std::string LOSE = "LOSE";
    const std::string ERROR = "ERROR";
}

bool send_ping_start(int session_id);
bool send_ping_guess(int session_id, char letter);
bool send_pong(int session_id, const std::string& game_state, int errors_left, const std::string& status, const std::string& additional_info = "");

struct ParsedMessage {
    std::string message_type;
    int session_id;
    std::string payload;
    std::string game_state;
    int errors_left;
    std::string status;
    std::string additional_info;
};

ParsedMessage read_next_message(int timeout_ms = 5000);
ParsedMessage wait_for_pong(int session_id, int timeout_ms = 5000);
ParsedMessage parse_message_from_string(const std::string& raw_message);

void clear_messages();
bool is_ping_message(const ParsedMessage& msg);
bool is_pong_message(const ParsedMessage& msg);

} 

#endif