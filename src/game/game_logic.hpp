#ifndef GAME_LOGIC_HPP
#define GAME_LOGIC_HPP

#include <string>
#include <vector>
#include <unordered_set>

namespace GameLogic {

class HangmanGame {
private:
    std::string secret_word_;
    std::string display_word_;  // Например: "c**c**t" для "circuit"
    std::unordered_set<char> guessed_letters_;
    int max_errors_;
    int current_errors_;
    bool game_over_;
    bool game_won_;

public:
    HangmanGame();
    
    // Инициализация новой игры
    void start_new_game(const std::string& word, int max_errors = 6);
    
    // Попытка угадать букву
    bool guess_letter(char letter);
    
    // Геттеры
    const std::string& get_display_word() const { return display_word_; }
    int get_errors_left() const { return max_errors_ - current_errors_; }
    int get_max_errors() const { return max_errors_; }
    bool is_game_over() const { return game_over_; }
    bool is_game_won() const { return game_won_; }
    const std::string& get_secret_word() const { return secret_word_; }
    const std::unordered_set<char>& get_guessed_letters() const { return guessed_letters_; }
    
    // Вспомогательные методы
    std::string get_wrong_letters() const;
    void update_display_word();
};

// Утилиты для работы со словарем
namespace Dictionary {
    std::vector<std::string> load_words(const std::string& filename);
    std::string get_random_word(const std::vector<std::string>& words);
}

} // namespace GameLogic

#endif