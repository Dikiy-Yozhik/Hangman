#ifndef FILE_HANDLE_HPP
#define FILE_HANDLE_HPP

#include <string>
#include <windows.h>

namespace FileSocket {

class FileHandle {
private:
    HANDLE handle_;
    std::string filename_;
    
public:
    FileHandle(const std::string& filename, DWORD desiredAccess, DWORD shareMode);
    ~FileHandle();
    
    bool is_valid() const { return handle_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return handle_; }
    const std::string& get_filename() const { return filename_; }
    
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    
    FileHandle(FileHandle&& other) noexcept;
    FileHandle& operator=(FileHandle&& other) noexcept;
};

}

#endif