// src/game_client.cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include "protocol/protocol.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int get_session_id() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return getpid();
#endif
}

class GameClient {
private:
    int session_id_;
    std::vector<char> guessed_letters_;
    
public:
    GameClient() : session_id_(get_session_id()) {}
    
    void display_game_state(const Protocol::ParsedMessage& response) {
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
        
        // Начинаем игру
        if (!start_new_game()) {
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
            if (!make_guess(letter)) {
                std::cout << "Game session ended." << std::endl;
                break;
            }
        }
    }
    
private:
    bool start_new_game() {
        Protocol::clear_messages();
        
        std::cout << "Starting new game..." << std::endl;
        if (!Protocol::send_ping_start(session_id_)) {
            std::cout << "Failed to send start request!" << std::endl;
            return false;
        }
        
        std::cout << "Waiting for game to start..." << std::endl;
        auto response = Protocol::wait_for_pong(session_id_, 5000);
        
        if (response.message_type.empty()) {
            std::cout << "No response from server!" << std::endl;
            return false;
        }
        
        if (response.status == "ERROR") {
            std::cout << "Server error: " << response.additional_info << std::endl;
            return false;
        }
        
        display_game_state(response);
        guessed_letters_.clear(); // Очищаем историю для новой игры
        
        Protocol::clear_messages();
        return true;
    }
    
    bool make_guess(char letter) {
        Protocol::clear_messages();
        
        if (!Protocol::send_ping_guess(session_id_, letter)) {
            std::cout << "Failed to send guess!" << std::endl;
            return false;
        }
        
        auto response = Protocol::wait_for_pong(session_id_, 5000);
        
        if (response.message_type.empty()) {
            std::cout << "No response from server!" << std::endl;
            return false;
        }
        
        display_game_state(response);
        
        // Проверяем завершение игры
        if (response.status == Protocol::GameStatus::WIN || 
            response.status == Protocol::GameStatus::LOSE) {
            
            std::cout << "\n*** GAME OVER ***" << std::endl;
            if (response.status == Protocol::GameStatus::WIN) {
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
        
        Protocol::clear_messages();
        return true;
    }
};

int main() {
    GameClient client;
    client.play_game();
    return 0;
}