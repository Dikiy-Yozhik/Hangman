#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <functional>
#include <cstdint>
#include "protocol/protocol.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

uint32_t get_session_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;
    
    auto now = std::chrono::steady_clock::now();
    auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Комбинируем timestamp и случайное число
    return (static_cast<uint32_t>(time_ms) & 0xFFFF0000) | (dis(gen) & 0x0000FFFF);
}

void sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

class GameClient {
private:
    uint32_t session_id_;
    uint32_t sequence_number_;
    std::vector<char> guessed_letters_;
    const int OPERATION_TIMEOUT_MS = 10000; // 10 секунд
    const int CONNECTION_RETRIES = 3;
    
public:
    GameClient() : session_id_(get_session_id()), sequence_number_(1) {}
    
    void display_binary_game_state(const Protocol::GameState& game_state) {
        std::cout << "\n=== HANGMAN GAME ===" << std::endl;
        std::cout << "Word: " << game_state.display_word << std::endl;
        std::cout << "Errors left: " << static_cast<int>(game_state.errors_left) << "/6" << std::endl;
        
        // Конвертируем статус в строку
        std::string status_str;
        switch (game_state.status) {
            case Protocol::GameStatus::IN_PROGRESS: status_str = "IN_PROGRESS"; break;
            case Protocol::GameStatus::WIN: status_str = "WIN"; break;
            case Protocol::GameStatus::LOSE: status_str = "LOSE"; break;
            case Protocol::GameStatus::ERROR_STATE: status_str = "ERROR"; break;
            default: status_str = "UNKNOWN";
        }
        
        std::cout << "Status: " << status_str << std::endl;
        
        if (!game_state.additional_info.empty()) {
            std::cout << "Info: " << game_state.additional_info << std::endl;
        }
        
        if (!guessed_letters_.empty()) {
            std::cout << "Guessed letters: ";
            for (char letter : guessed_letters_) {
                std::cout << letter << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "====================" << std::endl;
    }
    
    void display_parsed_game_state(const Protocol::ParsedMessage& response) {
        std::cout << "\n=== HANGMAN GAME ===" << std::endl;
        std::cout << "Word: " << response.game_state << std::endl;
        std::cout << "Errors left: " << response.errors_left << "/6" << std::endl;
        std::cout << "Status: " << response.status << std::endl;
        
        if (!response.additional_info.empty()) {
            std::cout << "Info: " << response.additional_info << std::endl;
        }
        
        if (!guessed_letters_.empty()) {
            std::cout << "Guessed letters: ";
            for (char letter : guessed_letters_) {
                std::cout << letter << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "====================" << std::endl;
    }
    
    void play_game() {
        std::cout << "Welcome to Hangman! Session ID: " << session_id_ << std::endl;
        std::cout << "Using binary protocol..." << std::endl;
        
        // Начинаем игру
        if (!start_new_game_binary()) {
            std::cout << "Failed to start game!" << std::endl;
            return;
        }
        
        // Игровой цикл
        while (true) {
            std::cout << "\nEnter a letter (or 'quit' to exit): ";
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "quit") {
                std::cout << "Thanks for playing!" << std::endl;
                break;
            }
            
            if (input.length() != 1) {
                std::cout << "Please enter exactly one letter!" << std::endl;
                continue;
            }
            
            char letter = input[0];
            if (!std::isalpha(letter)) {
                std::cout << "Please enter a valid letter (a-z)!" << std::endl;
                continue;
            }
            
            // Добавляем букву в список использованных
            if (std::find(guessed_letters_.begin(), guessed_letters_.end(), letter) == guessed_letters_.end()) {
                guessed_letters_.push_back(letter);
            }
            
            // Отправляем букву на сервер
            if (!make_guess_binary(letter)) {
                std::cout << "Game session ended." << std::endl;
                break;
            }
        }
    }
    
private:
    bool start_new_game_binary() {
        for (int attempt = 0; attempt < CONNECTION_RETRIES; ++attempt) {
            Protocol::clear_messages();
            
            std::cout << "Starting new game (attempt " << (attempt + 1) << ")..." << std::endl;
            
            // Используем бинарный протокол
            if (!Protocol::send_binary_ping(session_id_, sequence_number_++, "start")) {
                std::cout << "Failed to send start request!" << std::endl;
                if (attempt < CONNECTION_RETRIES - 1) {
                    sleep_ms(1000 * (attempt + 1));
                    continue;
                }
                return false;
            }
            
            // Ждем ответ с таймаутом
            auto binary_response = Protocol::receive_binary_message(session_id_, OPERATION_TIMEOUT_MS);

            std::cout << "DEBUG: Client received response - session_id: " << binary_response.header.session_id 
                    << ", message_type: " << binary_response.header.message_type << std::endl;

            if (binary_response.header.session_id != 0) {
                if (binary_response.header.message_type == Protocol::MessageType::PONG) {
                    auto game_state = Protocol::parse_pong_payload(binary_response.payload);
                    
                    if (game_state.status == Protocol::GameStatus::ERROR_STATE) {
                        std::cout << "Server error: " << game_state.additional_info << std::endl;
                        return false;
                    }
                    
                    display_binary_game_state(game_state);
                    guessed_letters_.clear();
                    return true;
                }
            } else {
                std::cout << "DEBUG: No valid response received" << std::endl;
            }
            
            std::cout << "No response from server, retrying..." << std::endl;
            if (attempt < CONNECTION_RETRIES - 1) {
                sleep_ms(1000 * (attempt + 1));
            }
        }
        
        std::cout << "Failed to start game after " << CONNECTION_RETRIES << " attempts" << std::endl;
        return false;
    }
    
    bool make_guess_binary(char letter) {
        std::string letter_str(1, letter);
        
        if (!Protocol::send_binary_ping(session_id_, sequence_number_++, letter_str)) {
            std::cout << "Failed to send guess!" << std::endl;
            return false;
        }
        
        auto binary_response = Protocol::receive_binary_message(session_id_, OPERATION_TIMEOUT_MS);
        
        if (binary_response.header.session_id == 0) {
            std::cout << "No response from server!" << std::endl;
            return false;
        }
        
        if (binary_response.header.message_type == Protocol::MessageType::PONG) {
            auto game_state = Protocol::parse_pong_payload(binary_response.payload);
            display_binary_game_state(game_state);
            
            // Проверяем завершение игры
            if (game_state.status == Protocol::GameStatus::WIN || 
                game_state.status == Protocol::GameStatus::LOSE) {
                
                std::cout << "\n*** GAME OVER ***" << std::endl;
                if (game_state.status == Protocol::GameStatus::WIN) {
                    std::cout << "🎉 Congratulations! You won! 🎉" << std::endl;
                } else {
                    std::cout << "💀 Game over! Better luck next time! 💀" << std::endl;
                }
                
                std::cout << "\nPlay again? (y/n): ";
                std::string choice;
                std::getline(std::cin, choice);
                
                if (choice == "y" || choice == "Y") {
                    sequence_number_ = 1; // Сбрасываем sequence для новой игры
                    return start_new_game_binary();
                } else {
                    return false;
                }
            }
        }
        
        return true;
    }
    
    // Старые методы для обратной совместимости (можно удалить после тестирования)
    bool start_new_game() {
        for (int attempt = 0; attempt < CONNECTION_RETRIES; ++attempt) {
            Protocol::clear_messages();
            
            std::cout << "Starting new game (attempt " << (attempt + 1) << ")..." << std::endl;
            if (!Protocol::send_ping_start(session_id_)) {
                std::cout << "Failed to send start request!" << std::endl;
                if (attempt < CONNECTION_RETRIES - 1) {
                    sleep_ms(1000 * (attempt + 1));
                    continue;
                }
                return false;
            }
            
            auto response = Protocol::wait_for_pong(session_id_, OPERATION_TIMEOUT_MS);
            
            if (!response.message_type.empty()) {
                if (response.status == "ERROR") {
                    std::cout << "Server error: " << response.additional_info << std::endl;
                    return false;
                }
                
                display_parsed_game_state(response);
                guessed_letters_.clear();
                return true;
            }
            
            std::cout << "No response from server, retrying..." << std::endl;
        }
        
        std::cout << "Failed to start game after " << CONNECTION_RETRIES << " attempts" << std::endl;
        return false;
    }
    
    bool make_guess(char letter) {
        if (!Protocol::send_ping_guess(session_id_, letter)) {
            std::cout << "Failed to send guess!" << std::endl;
            return false;
        }
        
        auto response = Protocol::wait_for_pong(session_id_, OPERATION_TIMEOUT_MS);
        
        if (response.message_type.empty()) {
            std::cout << "No response from server!" << std::endl;
            return false;
        }
        
        display_parsed_game_state(response);
        
        if (response.status == "WIN" || response.status == "LOSE") {
            std::cout << "\n*** GAME OVER ***" << std::endl;
            if (response.status == "WIN") {
                std::cout << "🎉 Congratulations! You won! 🎉" << std::endl;
            } else {
                std::cout << "💀 Game over! Better luck next time! 💀" << std::endl;
            }
            
            std::cout << "\nPlay again? (y/n): ";
            std::string choice;
            std::getline(std::cin, choice);
            
            if (choice == "y" || choice == "Y") {
                return start_new_game();
            } else {
                return false;
            }
        }
        
        return true;
    }
};

int main() {
    GameClient client;
    client.play_game();
    return 0;
}