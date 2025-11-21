#ifndef FILE_SOCKET_HPP
#define FILE_SOCKET_HPP

#include <string>
#include <vector>

namespace FileSocket {
    bool write_message(const std::string& filename, const std::string& message);
    std::vector<std::string> read_messages(const std::string& filename);
    void clear_messages(const std::string& filename);
    std::vector<std::string> wait_for_message(const std::string& filename, int timeout_ms);
}

#endif