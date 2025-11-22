#ifndef FILE_LOCK_HPP
#define FILE_LOCK_HPP

#include <windows.h>

namespace FileSocket {

class FileLock {
private:
    HANDLE file_handle_;
    DWORD offset_;
    DWORD size_;
    bool is_locked_;
    
public:
    FileLock(HANDLE file_handle, DWORD offset, DWORD size, bool auto_lock = true);
    ~FileLock();
    
    bool lock(int max_retries = 3);
    bool unlock();
    bool is_locked() const { return is_locked_; }
    
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
};

} 

#endif