#include "session_manager.hpp"
#include <iostream>

GameSession* SessionManager::get_session(uint32_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    return nullptr;
}

GameSession* SessionManager::create_session(uint32_t session_id, const std::string& word) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "Creating session: " << session_id << " with word: " << word << std::endl;
    
    auto session = std::make_unique<GameSession>(session_id);
    session->start_new_game(word);
    
    GameSession* result = session.get();
    sessions_[session_id] = std::move(session);
    session_end_times_.erase(session_id);
    
    return result;
}

void SessionManager::mark_session_completed(uint32_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_end_times_[session_id] = std::chrono::steady_clock::now();
}

void SessionManager::remove_session(uint32_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
    session_end_times_.erase(session_id);
}

void SessionManager::cleanup_inactive_sessions() {
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

size_t SessionManager::get_session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}
