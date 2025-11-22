#include "file_socket.hpp"
#include "ipc_common.hpp"
#include "../protocol/protocol.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>

namespace FileSocket {

// ==================== Вспомогательные функции ====================

std::string get_last_windows_error() {
    DWORD error = GetLastError();
    if (error == 0) return "No error";
    
    LPSTR buffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                (LPSTR)&buffer, 0, NULL);
    
    std::string message(buffer, size);
    LocalFree(buffer);
    
    return "Error " + std::to_string(error) + ": " + message;
}

bool initialize_shared_file(const std::string& filename) {
    ScopedFileHandle file(filename, GENERIC_READ | GENERIC_WRITE, 
                         FILE_SHARE_READ | FILE_SHARE_WRITE);
    return file.is_valid();
}

// ==================== Старые текстовые функции (для совместимости) ====================

bool write_message(const std::string& filename, const std::string& message) {
    ScopedFileHandle file_handle(filename, GENERIC_READ | GENERIC_WRITE, 
                                FILE_SHARE_READ | FILE_SHARE_WRITE);
    
    if (!file_handle.is_valid()) {
        std::cerr << "Failed to open file '" << filename << "': " << get_last_windows_error() << std::endl;
        return false;
    }
    
    // Временная глобальная блокировка (до перехода на регионы)
    ScopedFileLock file_lock(file_handle.get(), 0, 0);
    
    if (!file_lock.is_locked()) {
        std::cerr << "Failed to lock file '" << filename << "': " << get_last_windows_error() << std::endl;
        return false;
    }
    
    SetFilePointer(file_handle.get(), 0, NULL, FILE_END);
    
    std::string message_with_newline = message + "\n";
    DWORD bytes_written;
    
    BOOL result = WriteFile(file_handle.get(), 
                          message_with_newline.c_str(),
                          static_cast<DWORD>(message_with_newline.length()),
                          &bytes_written,
                          NULL);
    
    if (!result || bytes_written != message_with_newline.length()) {
        std::cerr << "Failed to write to file '" << filename << "': " << get_last_windows_error() << std::endl;
        return false;
    }
    
    return true;
}

std::vector<std::string> read_messages(const std::string& filename) {
    std::vector<std::string> messages;
    
    ScopedFileHandle file_handle(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE);
    if (!file_handle.is_valid()) {
        return messages;
    }
    
    ScopedFileLock file_lock(file_handle.get(), 0, 0);
    if (!file_lock.is_locked()) {
        return messages;
    }
    
    SetFilePointer(file_handle.get(), 0, NULL, FILE_BEGIN);
    
    DWORD file_size = GetFileSize(file_handle.get(), NULL);
    if (file_size == 0 || file_size == INVALID_FILE_SIZE) {
        return messages;
    }
    
    std::vector<char> buffer(file_size + 1);
    DWORD bytes_read;
    
    if (ReadFile(file_handle.get(), buffer.data(), file_size, &bytes_read, NULL)) {
        buffer[bytes_read] = '\0';
        
        std::string content(buffer.data());
        size_t start = 0;
        size_t end = content.find('\n');
        
        while (end != std::string::npos) {
            std::string line = content.substr(start, end - start);
            if (!line.empty()) {
                messages.push_back(line);
            }
            start = end + 1;
            end = content.find('\n', start);
        }
    }
    
    return messages;
}

void clear_messages(const std::string& filename) {
    ScopedFileHandle file_handle(filename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    if (!file_handle.is_valid()) {
        return;
    }
    
    ScopedFileLock file_lock(file_handle.get(), 0, 0);
    if (file_lock.is_locked()) {
        SetFilePointer(file_handle.get(), 0, NULL, FILE_BEGIN);
        SetEndOfFile(file_handle.get());
    }
}

std::vector<std::string> wait_for_message(const std::string& filename, int timeout_ms) {
    auto start = std::chrono::steady_clock::now();
    const int check_interval_ms = 100;
    
    while (true) {
        auto messages = read_messages(filename);
        if (!messages.empty()) {
            return messages;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed > timeout_ms) {
            return {};
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }
}

// ==================== Новые функции для региональных операций ====================

bool write_to_region(const std::string& filename, uint32_t offset, const std::vector<char>& data, int max_retries) {
    (void)max_retries;
    if (!IPC::is_valid_region_offset(offset)) {
        std::cerr << "Invalid region offset: " << offset << std::endl;
        return false;
    }
    
    if (data.empty() || data.size() > IPC::CLIENT_TO_SERVER_SIZE) {
        std::cerr << "Invalid data size: " << data.size() << std::endl;
        return false;
    }
    
    ScopedFileHandle file_handle(filename, GENERIC_READ | GENERIC_WRITE, 
                                FILE_SHARE_READ | FILE_SHARE_WRITE);
    if (!file_handle.is_valid()) {
        std::cerr << "Failed to open file: " << get_last_windows_error() << std::endl;
        return false;
    }
    
    ScopedFileLock region_lock(file_handle.get(), offset, static_cast<DWORD>(data.size()));
    if (!region_lock.is_locked()) {
        std::cerr << "Failed to lock region at offset " << offset << ": " << get_last_windows_error() << std::endl;
        return false;
    }
    
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    
    DWORD bytes_written;
    BOOL result = WriteFile(file_handle.get(), data.data(), 
                          static_cast<DWORD>(data.size()), &bytes_written, NULL);
    
    if (!result || bytes_written != data.size()) {
        std::cerr << "Failed to write to region: " << get_last_windows_error() << std::endl;
        return false;
    }
    
    return true;
}

std::vector<char> read_from_region(const std::string& filename, uint32_t offset, uint32_t size, int max_retries) {
    (void)max_retries;
    if (!IPC::is_valid_region_offset(offset) || size == 0 || size > IPC::SESSION_REGION_SIZE) {
        return {};
    }
    
    ScopedFileHandle file_handle(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);  // ← Нужен WRITE доступ!
    if (!file_handle.is_valid()) {
        return {};
    }
    
    ScopedFileLock region_lock(file_handle.get(), offset, size);
    if (!region_lock.is_locked()) {
        return {};
    }
    
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    
    // Читаем заголовок чтобы проверить валидность
    Protocol::MessageHeader header;
    DWORD bytes_read;
    BOOL result = ReadFile(file_handle.get(), &header, sizeof(Protocol::MessageHeader), &bytes_read, NULL);
    
    if (!result || bytes_read != sizeof(Protocol::MessageHeader)) {
        return {};
    }
    
    if (header.session_id == 0 || header.payload_size > IPC::MAX_PAYLOAD_SIZE) {
        return {};
    }
    
    uint32_t total_message_size = sizeof(Protocol::MessageHeader) + header.payload_size;
    if (total_message_size > size) {
        return {};
    }
    
    // Читаем всё сообщение
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    std::vector<char> buffer(total_message_size);
    
    result = ReadFile(file_handle.get(), buffer.data(), total_message_size, &bytes_read, NULL);
    if (!result || bytes_read != total_message_size) {
        return {};
    }
    
    std::cout << "DEBUG: Read valid message, size: " << total_message_size << " bytes" << std::endl;
    
    // ← ВАЖНО: ОЧИСТКА РЕГИОНА ПОСЛЕ ЧТЕНИЯ
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    std::vector<char> empty_data(size, 0);
    DWORD bytes_written;
    WriteFile(file_handle.get(), empty_data.data(), size, &bytes_written, NULL);
    std::cout << "DEBUG: Cleared region at offset " << offset << std::endl;
    
    return buffer;
}

bool clear_region(const std::string& filename, uint32_t offset, uint32_t size) {
    if (!IPC::is_valid_region_offset(offset)) {
        return false;
    }
    
    std::vector<char> empty_data(size, 0);
    return write_to_region(filename, offset, empty_data);
}

// ==================== Специализированные функции для сессий ====================

bool write_to_client_region(uint32_t session_id, const std::vector<char>& data, int max_retries) {
    std::cout << "DEBUG: Writing to client region, session_id=" << session_id 
              << ", data_size=" << data.size() << std::endl;
    
    if (!IPC::is_valid_session_id(session_id)) {
        std::cout << "DEBUG: Invalid session_id" << std::endl;
        return false;
    }
    
    uint32_t offset = IPC::get_client_to_server_offset(session_id);
    std::cout << "DEBUG: Calculated offset=" << offset << std::endl;
    
    bool result = write_to_region(IPC::SOCKET_FILE, offset, data, max_retries);
    std::cout << "DEBUG: Write result=" << result << std::endl;
    
    return result;
}

bool write_to_server_region(uint32_t session_id, const std::vector<char>& data, int max_retries) {
    if (!IPC::is_valid_session_id(session_id)) {
        return false;
    }
    
    uint32_t offset = IPC::get_server_to_client_offset(session_id);
    return write_to_region(IPC::SOCKET_FILE, offset, data, max_retries);
}

std::vector<char> read_from_client_region(uint32_t session_id, int max_retries) {
    if (!IPC::is_valid_session_id(session_id)) {
        return {};
    }
    
    uint32_t offset = IPC::get_client_to_server_offset(session_id);
    return read_from_region(IPC::SOCKET_FILE, offset, IPC::CLIENT_TO_SERVER_SIZE, max_retries);
}

std::vector<char> read_from_server_region(uint32_t session_id, int max_retries) {
    if (!IPC::is_valid_session_id(session_id)) {
        return {};
    }
    
    uint32_t offset = IPC::get_server_to_client_offset(session_id);
    return read_from_region(IPC::SOCKET_FILE, offset, IPC::SERVER_TO_CLIENT_SIZE, max_retries);
}

} // namespace FileSocket