#ifndef FILE_SOCKET_HPP
#define FILE_SOCKET_HPP

#include <string>
#include <vector>
#include <memory>
#include <windows.h>
#include "ipc_common.hpp"

namespace FileSocket {

// ==================== RAII-обертки ====================

class ScopedFileHandle {
private:
    HANDLE handle_;
    std::string filename_;
    
public:
    ScopedFileHandle(const std::string& filename, DWORD desiredAccess, DWORD shareMode) 
        : filename_(filename) {
        handle_ = CreateFileA(filename.c_str(), desiredAccess, shareMode,
                            NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    
    ~ScopedFileHandle() {
        if (is_valid()) {
            CloseHandle(handle_);
        }
    }
    
    bool is_valid() const { return handle_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return handle_; }
    const std::string& get_filename() const { return filename_; }
    
    // Запрещаем копирование
    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;
    
    // Разрешаем перемещение
    ScopedFileHandle(ScopedFileHandle&& other) noexcept 
        : handle_(other.handle_), filename_(std::move(other.filename_)) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }
    
    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            if (is_valid()) {
                CloseHandle(handle_);
            }
            handle_ = other.handle_;
            filename_ = std::move(other.filename_);
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
};

class ScopedFileLock {
private:
    HANDLE file_handle_;
    DWORD offset_;
    DWORD size_;
    bool is_locked_;
    
public:
    ScopedFileLock(HANDLE file_handle, DWORD offset, DWORD size, bool auto_lock = true)
        : file_handle_(file_handle), offset_(offset), size_(size), is_locked_(false) {
        if (auto_lock) {
            lock();
        }
    }
    
    ~ScopedFileLock() {
        unlock();
    }
    
    bool lock(int max_retries = 3) {
        if (is_locked_ || file_handle_ == INVALID_HANDLE_VALUE) return false;
        
        for (int attempt = 0; attempt < max_retries; ++attempt) {
            OVERLAPPED ov = {};
            ov.Offset = offset_;
            
            if (LockFileEx(file_handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, size_, 0, &ov)) {
                is_locked_ = true;
                return true;
            }
            
            if (attempt < max_retries - 1) {
                Sleep(IPC::RETRY_DELAY_MS * (attempt + 1));
            }
        }
        
        return false;
    }
    
    bool unlock() {
        if (!is_locked_ || file_handle_ == INVALID_HANDLE_VALUE) return false;
        
        OVERLAPPED ov = {};
        ov.Offset = offset_;
        BOOL result = UnlockFileEx(file_handle_, 0, size_, 0, &ov);
        is_locked_ = false;
        return result;
    }
    
    bool is_locked() const { return is_locked_; }
    
    // Запрещаем копирование
    ScopedFileLock(const ScopedFileLock&) = delete;
    ScopedFileLock& operator=(const ScopedFileLock&) = delete;
};

// ==================== Функции для работы с файлом ====================

// Старые текстовые функции (для обратной совместимости)
bool write_message(const std::string& filename, const std::string& message);
std::vector<std::string> read_messages(const std::string& filename);
void clear_messages(const std::string& filename);
std::vector<std::string> wait_for_message(const std::string& filename, int timeout_ms = IPC::READ_TIMEOUT_MS);

// Новые функции для региональных операций
bool write_to_region(const std::string& filename, uint32_t offset, const std::vector<char>& data, int max_retries = IPC::MAX_RETRY_ATTEMPTS);
std::vector<char> read_from_region(const std::string& filename, uint32_t offset, uint32_t size, int max_retries = IPC::MAX_RETRY_ATTEMPTS);
bool clear_region(const std::string& filename, uint32_t offset, uint32_t size);

// Специализированные функции для сессий
bool write_to_client_region(uint32_t session_id, const std::vector<char>& data, int max_retries = IPC::MAX_RETRY_ATTEMPTS);
bool write_to_server_region(uint32_t session_id, const std::vector<char>& data, int max_retries = IPC::MAX_RETRY_ATTEMPTS);
std::vector<char> read_from_client_region(uint32_t session_id, int max_retries = IPC::MAX_RETRY_ATTEMPTS);
std::vector<char> read_from_server_region(uint32_t session_id, int max_retries = IPC::MAX_RETRY_ATTEMPTS);

// Вспомогательные функции
std::string get_last_windows_error();
bool initialize_shared_file(const std::string& filename);

} // namespace FileSocket

#endif // FILE_SOCKET_HPP