// src/ipc/file_socket.cpp
#include "ipc_common.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <windows.h>

namespace FileSocket {

    struct FileLockContext {
        HANDLE handle = INVALID_HANDLE_VALUE;
        bool is_locked = false;
    };

    // Блокировка файла 
    FileLockContext lock_file(const std::string& filename) {
        FileLockContext context;
        
        context.handle = CreateFileA(filename.c_str(), 
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, 
                                    OPEN_ALWAYS, 
                                    FILE_ATTRIBUTE_NORMAL, 
                                    NULL);
        
        if (context.handle != INVALID_HANDLE_VALUE) {
            OVERLAPPED ov = {};
            context.is_locked = LockFileEx(context.handle, 
                                        LOCKFILE_EXCLUSIVE_LOCK, 
                                        0, MAXDWORD, MAXDWORD, &ov);
            
            if (!context.is_locked) {
                CloseHandle(context.handle);
                context.handle = INVALID_HANDLE_VALUE;
            }
        }
        
        return context;
    }

    // Разблокировка файла
    bool unlock_file(FileLockContext& context) {
        if (context.handle != INVALID_HANDLE_VALUE && context.is_locked) {
            OVERLAPPED ov = {};
            BOOL result = UnlockFileEx(context.handle, 0, MAXDWORD, MAXDWORD, &ov);
            CloseHandle(context.handle);
            context.handle = INVALID_HANDLE_VALUE;
            context.is_locked = false;
            return result;
        }
        return false;
    }

    // Запись в файл 
    bool write_to_locked_file(HANDLE file_handle, const std::string& message) {
        SetFilePointer(file_handle, 0, NULL, FILE_END);
        
        std::string message_with_newline = message + "\n";
        DWORD bytes_written;
        
        BOOL result = WriteFile(file_handle, 
                            message_with_newline.c_str(),
                            message_with_newline.length(),
                            &bytes_written,
                            NULL);
        
        return result && (bytes_written == message_with_newline.length());
    }

    bool write_message(const std::string& filename, const std::string& message) {
        FileLockContext lock = lock_file(filename);
        if (!lock.is_locked) {
            std::cerr << "Failed to lock file for writing: " << filename << std::endl;
            return false;
        }
        
        bool success = write_to_locked_file(lock.handle, message);
        
        unlock_file(lock);
        return success;
    }

    // Чтение из файла 
    std::vector<std::string> read_from_locked_file(HANDLE file_handle) {
        std::vector<std::string> messages;
        
        SetFilePointer(file_handle, 0, NULL, FILE_BEGIN);
        
        DWORD file_size = GetFileSize(file_handle, NULL);
        if (file_size == 0) {
            return messages;
        }
        
        std::vector<char> buffer(file_size + 1);
        DWORD bytes_read;
        
        if (ReadFile(file_handle, buffer.data(), file_size, &bytes_read, NULL)) {
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

    std::vector<std::string> read_messages(const std::string& filename) {
        std::vector<std::string> messages;
        
        FileLockContext lock = lock_file(filename);
        if (!lock.is_locked) {
            return messages;
        }
        
        messages = read_from_locked_file(lock.handle);
        
        unlock_file(lock);
        return messages;
    }

    // Очистка сообщений
    void clear_messages(const std::string& filename) {
        FileLockContext lock = lock_file(filename);
        if (lock.is_locked) {
            SetFilePointer(lock.handle, 0, NULL, FILE_BEGIN);
            SetEndOfFile(lock.handle);
            unlock_file(lock);
        }
    }

    // Ожидание сообщения
    std::vector<std::string> wait_for_message(const std::string& filename, int timeout_ms = IPC::READ_TIMEOUT) {
        auto start = std::chrono::steady_clock::now();
        
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
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

} 
