#ifndef GAME_SESSION_HPP
#define GAME_SESSION_HPP

#include <cstdint>
#include <string>
#include "../protocol/protocol.hpp"
#include "../game/game_logic.hpp"

class GameSession {
private:
    uint32_t session_id_;
    GameLogic::HangmanGame game_;
    uint32_t last_processed_sequence_;
    
public:
    GameSession(uint32_t session_id);
    
    bool should_process_message(uint32_t sequence);
    void update_sequence(uint32_t sequence);
    void start_new_game(const std::string& word);
    Protocol::GameState process_guess(char letter);
    Protocol::GameState get_current_state();
    bool is_game_active() const;
    uint32_t get_session_id() const { return session_id_; }
};

#endif
