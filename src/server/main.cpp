#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "session_manager.hpp"
#include "../protocol/protocol.hpp"
#include "../game/game_logic.hpp"

int main() {
    std::cout << "Starting Hangman Server..." << std::endl;
    
    auto words = GameLogic::Dictionary::load_words("resources/words.txt");
    if (words.empty()) {
        std::cout << "Error: No words loaded!" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << words.size() << " words" << std::endl;
    
    SessionManager session_manager;
    auto last_cleanup_time = std::chrono::steady_clock::now();
    uint32_t sequence_counter = 1;
    
    while (true) {
        std::cout << "Active sessions: " << session_manager.get_session_count() << std::endl;
        std::cout << "Waiting for messages..." << std::endl;
        
        auto binary_message = Protocol::receive_binary_message(0, 5000);
        
        if (binary_message.header.session_id != 0) {
            std::cout << "Processing message from session " << binary_message.header.session_id 
                      << ", sequence " << binary_message.header.sequence 
                      << ", type " << binary_message.header.message_type << std::endl;
            
            auto session = session_manager.get_session(binary_message.header.session_id);
            
            if (binary_message.header.message_type == Protocol::MessageType::PING) {
                if (!Protocol::validate_session_id(binary_message.header.session_id)) {
                    std::cout << "Invalid session ID: " << binary_message.header.session_id << std::endl;
                    continue;
                }
                
                std::string payload = Protocol::parse_ping_payload(binary_message.payload);
                
                if (!Protocol::validate_ping_payload(payload)) {
                    std::cout << "Invalid PING payload from session " << binary_message.header.session_id 
                              << ": " << payload << std::endl;
                    
                    Protocol::GameState error_state;
                    error_state.display_word = "";
                    error_state.errors_left = 0;
                    error_state.status = Protocol::GameStatus::ERROR_STATE;
                    error_state.additional_info = "Invalid message format";
                    
                    Protocol::send_binary_pong(binary_message.header.session_id, 
                                             sequence_counter++, error_state);
                    continue;
                }
                
                if (payload == "start") {
                    if (session && session->is_game_active()) {
                        std::cout << "Game already in progress for session " << binary_message.header.session_id << std::endl;
                        
                        Protocol::GameState error_state;
                        error_state.display_word = "";
                        error_state.errors_left = 0;
                        error_state.status = Protocol::GameStatus::ERROR_STATE;
                        error_state.additional_info = "Game already in progress";
                        
                        Protocol::send_binary_pong(binary_message.header.session_id, 
                                                 sequence_counter++, error_state);
                        continue;
                    }
                    
                    std::string word = GameLogic::Dictionary::get_random_word(words);
                    session = session_manager.create_session(binary_message.header.session_id, word);
                    
                    std::cout << "Started new game with word: " << word << std::endl;
                    
                    auto initial_state = session->get_current_state();
                    initial_state.additional_info = "Game started! Guess a letter.";
                    
                    Protocol::send_binary_pong(binary_message.header.session_id, 
                                             sequence_counter++, initial_state);
                    
                } else if (payload.length() == 1 && session) {
                    if (!session->should_process_message(binary_message.header.sequence)) {
                        std::cout << "Duplicate message from session " 
                                  << binary_message.header.session_id << std::endl;
                        continue;
                    }
                    
                    session->update_sequence(binary_message.header.sequence);
                    
                    char letter = payload[0];
                    auto game_state = session->process_guess(letter);
                    
                    Protocol::send_binary_pong(binary_message.header.session_id, 
                                             sequence_counter++, game_state);
                    
                    std::cout << "Processed guess '" << letter << "' for session " 
                              << binary_message.header.session_id << std::endl;
                    
                    if (!session->is_game_active()) {
                        session_manager.mark_session_completed(binary_message.header.session_id);
                        std::cout << "Game completed for session " << binary_message.header.session_id << std::endl;
                    }
                } else if (!session) {
                    Protocol::GameState error_state;
                    error_state.display_word = "";
                    error_state.errors_left = 0;
                    error_state.status = Protocol::GameStatus::ERROR_STATE;
                    error_state.additional_info = "No active game session. Send 'start' to begin.";
                    
                    Protocol::send_binary_pong(binary_message.header.session_id, 
                                             sequence_counter++, error_state);
                }
            }
        }
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup_time).count() >= 10) {
            session_manager.cleanup_inactive_sessions();
            last_cleanup_time = now;
        }
    }
    
    return 0;
}
