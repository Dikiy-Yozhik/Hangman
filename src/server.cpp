// src/game_server.cpp
#include <iostream>
#include <string>
#include <vector>
#include "protocol/protocol.hpp"
#include "game/game_logic.hpp"

class GameSession {
private:
    int session_id_;
    GameLogic::HangmanGame game_;
    
public:
    GameSession(int session_id) : session_id_(session_id) {}
    
    void start_new_game(const std::string& word) {
        game_.start_new_game(word);
    }
    
    Protocol::ParsedMessage process_guess(char letter) {
        bool correct = game_.guess_letter(letter);
        
        std::string status = Protocol::GameStatus::IN_PROGRESS;
        std::string additional_info;
        
        if (game_.is_game_won()) {
            status = Protocol::GameStatus::WIN;
            additional_info = "You won! The word was: " + game_.get_secret_word();
        } else if (game_.is_game_over()) {
            status = Protocol::GameStatus::LOSE;
            additional_info = "You lost! The word was: " + game_.get_secret_word();
        } else {
            if (!correct) {
                additional_info = "Wrong! Wrong letters: " + game_.get_wrong_letters();
            } else {
                additional_info = "Correct! Wrong letters: " + game_.get_wrong_letters();
            }
        }
        
        return Protocol::ParsedMessage{
            Protocol::MessageType::PONG,
            session_id_,
            "", // payload не нужен для PONG
            game_.get_display_word(),
            game_.get_errors_left(),
            status,
            additional_info
        };
    }
    
    bool is_game_active() const {
        return !game_.is_game_over() && !game_.is_game_won();
    }
    
    int get_session_id() const { return session_id_; }
};

int main() {
    std::cout << "Starting Hangman Server..." << std::endl;
    
    // Загружаем словарь
    auto words = GameLogic::Dictionary::load_words("resources/words.txt");
    if (words.empty()) {
        std::cout << "Error: No words loaded!" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << words.size() << " words" << std::endl;
    
    Protocol::clear_messages();
    
    GameSession* current_session = nullptr;
    
    while (true) {
        std::cout << "Waiting for messages..." << std::endl;
        
        auto message = Protocol::read_next_message(10000);
        
        if (!message.message_type.empty()) {
            if (message.message_type == Protocol::MessageType::PING) {
                std::cout << "Processing PING from session " << message.session_id << std::endl;
                
                if (message.payload == "start") {
                    // Начало новой игры
                    if (current_session && current_session->is_game_active()) {
                        std::cout << "Game already in progress for session " << message.session_id << std::endl;
                        continue;
                    }
                    
                    std::string word = GameLogic::Dictionary::get_random_word(words);
                    current_session = new GameSession(message.session_id);
                    current_session->start_new_game(word);
                    
                    std::cout << "Started new game with word: " << word << std::endl;
                    
                    // Отправляем начальное состояние
                    Protocol::send_pong(message.session_id,
                                      current_session->process_guess(' ').game_state, // Пустой ход для получения состояния
                                      6,
                                      Protocol::GameStatus::IN_PROGRESS,
                                      "Game started! Guess a letter.");
                    
                } else if (message.payload.length() == 1 && current_session) {
                    // Угадывание буквы
                    char letter = message.payload[0];
                    auto response = current_session->process_guess(letter);
                    
                    Protocol::send_pong(response.session_id,
                                      response.game_state,
                                      response.errors_left,
                                      response.status,
                                      response.additional_info);
                    
                    std::cout << "Processed guess '" << letter << "' for session " << message.session_id << std::endl;
                    
                    // Если игра завершена, очищаем сессию
                    if (!current_session->is_game_active()) {
                        delete current_session;
                        current_session = nullptr;
                    }
                }
                
                // Очищаем файл после обработки
                //Protocol::clear_messages();
            }
        }
    }
    
    if (current_session) {
        delete current_session;
    }
    
    return 0;
}