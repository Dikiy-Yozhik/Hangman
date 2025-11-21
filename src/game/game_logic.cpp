// src/game/game_logic.cpp
#include "game_logic.hpp"
#include <fstream>
#include <random>
#include <algorithm>

namespace GameLogic {

HangmanGame::HangmanGame() 
    : max_errors_(6), current_errors_(0), game_over_(false), game_won_(false) {
}

void HangmanGame::start_new_game(const std::string& word, int max_errors) {
    secret_word_ = word;
    max_errors_ = max_errors;
    current_errors_ = 0;
    game_over_ = false;
    game_won_ = false;
    guessed_letters_.clear();
    
    // Инициализируем display_word звездочками
    display_word_ = std::string(secret_word_.length(), '*');
}

bool HangmanGame::guess_letter(char letter) {
    if (game_over_ || game_won_) return false;
    
    char lower_letter = std::tolower(letter);
    
    // Проверяем, не угадывали ли уже эту букву
    if (guessed_letters_.count(lower_letter) > 0) {
        return false; // Буква уже была
    }
    
    guessed_letters_.insert(lower_letter);
    
    // Проверяем есть ли буква в слове
    bool letter_found = false;
    for (char c : secret_word_) {
        if (std::tolower(c) == lower_letter) {
            letter_found = true;
            break;
        }
    }
    
    if (letter_found) {
        // Обновляем display_word
        update_display_word();
        
        // Проверяем победу
        if (display_word_ == secret_word_) {
            game_won_ = true;
            game_over_ = true;
        }
    } else {
        // Буквы нет - увеличиваем ошибки
        current_errors_++;
        if (current_errors_ >= max_errors_) {
            game_over_ = true;
        }
    }
    
    return letter_found;
}

void HangmanGame::update_display_word() {
    display_word_.clear();
    for (char c : secret_word_) {
        char lower_c = std::tolower(c);
        if (guessed_letters_.count(lower_c) > 0) {
            display_word_ += c; // Показываем угаданную букву
        } else {
            display_word_ += '*'; // Скрываем неугаданную
        }
    }
}

std::string HangmanGame::get_wrong_letters() const {
    std::string wrong_letters;
    for (char letter : guessed_letters_) {
        // Проверяем, есть ли буква в секретном слове
        bool found = false;
        for (char c : secret_word_) {
            if (std::tolower(c) == letter) {
                found = true;
                break;
            }
        }
        
        // Добавляем ТОЛЬКО если буква НЕ найдена в слове
        if (!found) {
            if (!wrong_letters.empty()) wrong_letters += ", ";
            wrong_letters += letter;
        }
    }
    return wrong_letters;
}

// Реализация утилит словаря
std::vector<std::string> Dictionary::load_words(const std::string& filename) {
    std::vector<std::string> words;
    std::ifstream file(filename);
    std::string word;
    
    while (std::getline(file, word)) {
        if (!word.empty()) {
            words.push_back(word);
        }
    }
    
    return words;
}

std::string Dictionary::get_random_word(const std::vector<std::string>& words) {
    if (words.empty()) {
        return "hangman"; // fallback
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, words.size() - 1);
    
    return words[dist(gen)];
}

} // namespace GameLogic
