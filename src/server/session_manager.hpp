#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include "game_session.hpp"

class SessionManager {
private:
    std::unordered_map<uint32_t, std::unique_ptr<GameSession>> sessions_;
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> session_end_times_;

public:
    SessionManager() = default;
    
    GameSession* get_session(uint32_t session_id);
    GameSession* create_session(uint32_t session_id, const std::string& word);
    void mark_session_completed(uint32_t session_id);
    void remove_session(uint32_t session_id);
    void cleanup_inactive_sessions();
    size_t get_session_count() const;
};

#endif
