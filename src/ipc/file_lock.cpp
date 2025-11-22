#include "file_lock.hpp"
#include <windows.h>
#include <thread>

namespace FileSocket {

FileLock::FileLock(HANDLE file_handle, DWORD offset, DWORD size, bool auto_lock)
    : file_handle_(file_handle), offset_(offset), size_(size), is_locked_(false) {
    if (auto_lock) {
        lock();
    }
}

FileLock::~FileLock() {
    unlock();
}

bool FileLock::lock(int max_retries) {
    if (is_locked_ || file_handle_ == INVALID_HANDLE_VALUE) return false;
    
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        OVERLAPPED ov = {};
        ov.Offset = offset_;
        
        if (LockFileEx(file_handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, size_, 0, &ov)) {
            is_locked_ = true;
            return true;
        }
        
        if (attempt < max_retries - 1) {
            Sleep(100 * (attempt + 1));
        }
    }
    
    return false;
}

bool FileLock::unlock() {
    if (!is_locked_ || file_handle_ == INVALID_HANDLE_VALUE) return false;
    
    OVERLAPPED ov = {};
    ov.Offset = offset_;
    BOOL result = UnlockFileEx(file_handle_, 0, size_, 0, &ov);
    is_locked_ = false;
    return result;
}

} 
