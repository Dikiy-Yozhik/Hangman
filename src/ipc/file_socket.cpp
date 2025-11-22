#include "file_socket.hpp"
#include "region_ops.hpp"
#include <iostream>

namespace FileSocket {

bool write_to_client_region(uint32_t session_id, const std::vector<char>& data) {
    if (!IPC::is_valid_session_id(session_id)) {
        return false;
    }
    
    uint32_t offset = IPC::get_client_to_server_offset(session_id);
    return write_to_region_impl(IPC::SOCKET_FILE, offset, data);
}

bool write_to_server_region(uint32_t session_id, const std::vector<char>& data) {
    if (!IPC::is_valid_session_id(session_id)) {
        return false;
    }
    
    uint32_t offset = IPC::get_server_to_client_offset(session_id);
    return write_to_region_impl(IPC::SOCKET_FILE, offset, data);
}

std::vector<char> read_from_client_region(uint32_t session_id) {
    if (!IPC::is_valid_session_id(session_id)) {
        return {};
    }
    
    uint32_t offset = IPC::get_client_to_server_offset(session_id);
    return read_from_region_impl(IPC::SOCKET_FILE, offset, IPC::CLIENT_TO_SERVER_SIZE);
}

std::vector<char> read_from_server_region(uint32_t session_id) {
    if (!IPC::is_valid_session_id(session_id)) {
        return {};
    }
    
    uint32_t offset = IPC::get_server_to_client_offset(session_id);
    return read_from_region_impl(IPC::SOCKET_FILE, offset, IPC::SERVER_TO_CLIENT_SIZE);
}

} 
