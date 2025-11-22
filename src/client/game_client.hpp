#ifndef GAME_CLIENT_HPP
#define GAME_CLIENT_HPP

#include <cstdint>
#include <vector>
#include <string>
#include "../protocol/protocol.hpp"

class GameClient {
private:
    uint32_t session_id_;
    uint32_t sequence_number_;
    std::vector<char> guessed_letters_;
    const int OPERATION_TIMEOUT_MS = 10000;
    const int CONNECTION_RETRIES = 3;
    
    void display_game_state(const Protocol::GameState& game_state);
    bool start_new_game();
    bool make_guess(char letter);
    
public:
    GameClient();
    void play_game();
};

uint32_t gen_session_id();
void sleep_ms(int milliseconds);

#endif
