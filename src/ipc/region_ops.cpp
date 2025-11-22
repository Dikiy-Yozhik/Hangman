#include "region_ops.hpp"
#include "../protocol/protocol.hpp"
#include <iostream>
#include <cstring>

namespace FileSocket {

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

bool write_to_region_impl(const std::string& filename, uint32_t offset, const std::vector<char>& data) {
    if (!IPC::is_valid_region_offset(offset)) {
        return false;
    }
    
    if (data.empty() || data.size() > IPC::CLIENT_TO_SERVER_SIZE) {
        return false;
    }
    
    FileHandle file_handle(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    if (!file_handle.is_valid()) {
        return false;
    }
    
    FileLock region_lock(file_handle.get(), offset, static_cast<DWORD>(data.size()));
    if (!region_lock.is_locked()) {
        return false;
    }
    
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    
    DWORD bytes_written;
    BOOL result = WriteFile(file_handle.get(), data.data(), 
                          static_cast<DWORD>(data.size()), &bytes_written, NULL);
    
    return result && (bytes_written == data.size());
}

std::vector<char> read_from_region_impl(const std::string& filename, uint32_t offset, uint32_t size) {
    if (!IPC::is_valid_region_offset(offset) || size == 0 || size > IPC::SESSION_REGION_SIZE) {
        return {};
    }
    
    FileHandle file_handle(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    if (!file_handle.is_valid()) {
        return {};
    }
    
    FileLock region_lock(file_handle.get(), offset, size);
    if (!region_lock.is_locked()) {
        return {};
    }
    
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    
    // Читаем заголовок для проверки валидности
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
    
    // Очищаем регион после чтения
    SetFilePointer(file_handle.get(), offset, NULL, FILE_BEGIN);
    std::vector<char> empty_data(size, 0);
    DWORD bytes_written;
    WriteFile(file_handle.get(), empty_data.data(), size, &bytes_written, NULL);
    
    return buffer;
}

} 
