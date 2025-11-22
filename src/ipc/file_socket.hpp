#ifndef FILE_SOCKET_HPP
#define FILE_SOCKET_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "ipc_common.hpp"

#include "file_handle.hpp"
#include "file_lock.hpp"

namespace FileSocket {

bool write_to_client_region(uint32_t session_id, const std::vector<char>& data);
bool write_to_server_region(uint32_t session_id, const std::vector<char>& data);
std::vector<char> read_from_client_region(uint32_t session_id);
std::vector<char> read_from_server_region(uint32_t session_id);

} 

#endif