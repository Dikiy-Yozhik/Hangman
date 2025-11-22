#include "game_client.hpp"
#include <iostream>

int main() {
    try {
        GameClient client;
        client.play_game();
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
