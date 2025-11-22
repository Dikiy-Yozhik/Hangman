#include "game_session.hpp"

GameSession::GameSession(uint32_t session_id) : session_id_(session_id), last_processed_sequence_(0) {}

bool GameSession::should_process_message(uint32_t sequence) {
    return sequence > last_processed_sequence_;
}

void GameSession::update_sequence(uint32_t sequence) {
    last_processed_sequence_ = sequence;
}

void GameSession::start_new_game(const std::string& word) {
    game_.start_new_game(word);
}

Protocol::GameState GameSession::process_guess(char letter) {
    bool correct = game_.guess_letter(letter);
    
    Protocol::GameState game_state;
    game_state.display_word = game_.get_display_word();
    game_state.errors_left = static_cast<uint8_t>(game_.get_errors_left());
    
    if (game_.is_game_won()) {
        game_state.status = Protocol::GameStatus::WIN;
        game_state.additional_info = "You won! The word was: " + game_.get_secret_word();
    } else if (game_.is_game_over()) {
        game_state.status = Protocol::GameStatus::LOSE;
        game_state.additional_info = "You lost! The word was: " + game_.get_secret_word();
    } else {
        game_state.status = Protocol::GameStatus::IN_PROGRESS;
        if (!correct) {
            game_state.additional_info = "Wrong! Wrong letters: " + game_.get_wrong_letters();
        } else {
            game_state.additional_info = "Correct! Wrong letters: " + game_.get_wrong_letters();
        }
    }
    
    return game_state;
}

Protocol::GameState GameSession::get_current_state() {
    Protocol::GameState game_state;
    game_state.display_word = game_.get_display_word();
    game_state.errors_left = static_cast<uint8_t>(game_.get_errors_left());
    game_state.status = Protocol::GameStatus::IN_PROGRESS;
    game_state.additional_info = "Game in progress";
    return game_state;
}

bool GameSession::is_game_active() const {
    return !game_.is_game_over() && !game_.is_game_won();
}
