#ifndef IPC_COMMON_HPP
#define IPC_COMMON_HPP

#include <string>
#include <cstdint>

namespace IPC {
    // Основные константы
    const std::string SOCKET_FILE = "hangman_socket.txt";
    const int MAX_MESSAGE_SIZE = 256;
    const int MAX_SESSIONS = 10;
    const int SESSION_REGION_SIZE = 1024;    // 1KB на сессию
    const int FILE_HEADER_SIZE = 128;
    const int CLIENT_TO_SERVER_SIZE = 512;   // Половина региона для клиента→сервера
    const int SERVER_TO_CLIENT_SIZE = 512;   // Половина для сервера→клиента
    const int LOCK_TIMEOUT_MS = 5000;
    const int READ_TIMEOUT_MS = 10000;
    const int MAX_RETRY_ATTEMPTS = 3;
    const int RETRY_DELAY_MS = 100;
    
    // Константы для бинарного протокола
    const int BINARY_HEADER_SIZE = 20;  // sizeof(MessageHeader) с выравниванием
    const int MAX_PAYLOAD_SIZE = MAX_MESSAGE_SIZE - BINARY_HEADER_SIZE;
    
    // Вспомогательные функции
    inline bool is_valid_session_id(uint32_t session_id) {
        return session_id != 0 && session_id != UINT32_MAX;
    }
    
    inline uint32_t get_session_file_offset(uint32_t session_id) {
        if (!is_valid_session_id(session_id)) {
            return FILE_HEADER_SIZE; // fallback to first session
        }
        return FILE_HEADER_SIZE + (session_id % MAX_SESSIONS) * SESSION_REGION_SIZE;
    }
    
    inline uint32_t get_client_to_server_offset(uint32_t session_id) {
        return get_session_file_offset(session_id);
    }
    
    inline uint32_t get_server_to_client_offset(uint32_t session_id) {
        uint32_t base_offset = get_session_file_offset(session_id);
        return base_offset + CLIENT_TO_SERVER_SIZE;
    }
    
    inline bool is_valid_region_offset(uint32_t offset) {
        return offset >= FILE_HEADER_SIZE && 
               offset < FILE_HEADER_SIZE + (MAX_SESSIONS * SESSION_REGION_SIZE);
    }
}

#endif // IPC_COMMON_HPP
