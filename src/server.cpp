#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <cstdint>
#include "protocol/protocol.hpp"
#include "game/game_logic.hpp"

class GameSession {
private:
    uint32_t session_id_;
    GameLogic::HangmanGame game_;
    uint32_t last_processed_sequence_;
    
public:
    GameSession(uint32_t session_id) : session_id_(session_id), last_processed_sequence_(0) {}
    
    bool should_process_message(uint32_t sequence) {
        return sequence > last_processed_sequence_;
    }
    
    void update_sequence(uint32_t sequence) {
        last_processed_sequence_ = sequence;
    }
    
    void start_new_game(const std::string& word) {
        game_.start_new_game(word);
    }
    
    Protocol::GameState process_guess(char letter) {
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
    
    Protocol::GameState get_current_state() {
        Protocol::GameState game_state;
        game_state.display_word = game_.get_display_word();
        game_state.errors_left = static_cast<uint8_t>(game_.get_errors_left());
        game_state.status = Protocol::GameStatus::IN_PROGRESS;
        game_state.additional_info = "Game in progress";
        return game_state;
    }
    
    bool is_game_active() const {
        return !game_.is_game_over() && !game_.is_game_won();
    }
    
    uint32_t get_session_id() const { return session_id_; }
};

class SessionManager {
private:
    std::unordered_map<uint32_t, std::unique_ptr<GameSession>> sessions_;
    mutable std::mutex mutex_;  // mutable для const методов
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> session_end_times_;

public:
    SessionManager() = default;
    
    GameSession* get_session(uint32_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second.get();
        }
        
        std::cout << "Session not found: " << session_id << std::endl;
        return nullptr;
    }
    
    GameSession* create_session(uint32_t session_id, const std::string& word) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "Creating session: " << session_id << " with word: " << word << std::endl;
        
        auto session = std::make_unique<GameSession>(session_id);
        session->start_new_game(word);
        
        // Сохраняем указатель до перемещения
        GameSession* result = session.get();
        sessions_[session_id] = std::move(session);
        session_end_times_.erase(session_id);
        
        return result;
    }
    
    void mark_session_completed(uint32_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        session_end_times_[session_id] = std::chrono::steady_clock::now();
    }
    
    void remove_session(uint32_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(session_id);
        session_end_times_.erase(session_id);
    }
    
    void cleanup_inactive_sessions() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(30);
        
        for (auto it = session_end_times_.begin(); it != session_end_times_.end(); ) {
            if (now - it->second > timeout) {
                std::cout << "Cleaning up inactive session: " << it->first << std::endl;
                sessions_.erase(it->first);
                it = session_end_times_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    size_t get_session_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }
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
    
    SessionManager session_manager;
    auto last_cleanup_time = std::chrono::steady_clock::now();
    uint32_t sequence_counter = 1;
    
    while (true) {
        std::cout << "Active sessions: " << session_manager.get_session_count() << std::endl;
        std::cout << "Waiting for messages..." << std::endl;
        
        // Используем бинарный протокол для получения сообщений
        auto binary_message = Protocol::receive_binary_message(0, 5000); // session_id 0 для любого сообщения
        
        if (binary_message.header.session_id != 0) {
            std::cout << "Processing message from session " << binary_message.header.session_id 
                      << ", sequence " << binary_message.header.sequence 
                      << ", type " << binary_message.header.message_type << std::endl;
            
            auto session = session_manager.get_session(binary_message.header.session_id);
            
            if (binary_message.header.message_type == Protocol::MessageType::PING) {
                // Валидация session_id
                if (!Protocol::validate_session_id(binary_message.header.session_id)) {
                    std::cout << "Invalid session ID: " << binary_message.header.session_id << std::endl;
                    continue;
                }
                
                std::string payload = Protocol::parse_ping_payload(binary_message.payload);
                
                // Валидация payload
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
                    // Начало новой игры
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
                    
                    // Создаем новую сессию
                    std::string word = GameLogic::Dictionary::get_random_word(words);
                    session = session_manager.create_session(binary_message.header.session_id, word);
                    
                    std::cout << "Started new game with word: " << word << std::endl;
                    
                    // Отправляем начальное состояние
                    auto initial_state = session->get_current_state();
                    initial_state.additional_info = "Game started! Guess a letter.";
                    
                    Protocol::send_binary_pong(binary_message.header.session_id, 
                                             sequence_counter++, initial_state);
                    
                } else if (payload.length() == 1 && session) {
                    // Проверяем sequence number
                    if (!session->should_process_message(binary_message.header.sequence)) {
                        std::cout << "Duplicate or out-of-order message from session " 
                                  << binary_message.header.session_id << std::endl;
                        continue;
                    }
                    
                    session->update_sequence(binary_message.header.sequence);
                    
                    // Угадывание буквы
                    char letter = payload[0];
                    auto game_state = session->process_guess(letter);
                    
                    Protocol::send_binary_pong(binary_message.header.session_id, 
                                             sequence_counter++, game_state);
                    
                    std::cout << "Processed guess '" << letter << "' for session " 
                              << binary_message.header.session_id << std::endl;
                    
                    // Если игра завершена, помечаем сессию как завершенную
                    if (!session->is_game_active()) {
                        session_manager.mark_session_completed(binary_message.header.session_id);
                        std::cout << "Game completed for session " << binary_message.header.session_id << std::endl;
                    }
                } else if (!session) {
                    // Сессия не найдена
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
        
        // Очистка неактивных сессий каждые 10 секунд
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup_time).count() >= 10) {
            session_manager.cleanup_inactive_sessions();
            last_cleanup_time = now;
        }
    }
    
    return 0;
}