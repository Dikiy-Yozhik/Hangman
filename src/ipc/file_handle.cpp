#include "file_handle.hpp"
#include <windows.h>

namespace FileSocket {

FileHandle::FileHandle(const std::string& filename, DWORD desiredAccess, DWORD shareMode) 
    : filename_(filename) {
    handle_ = CreateFileA(filename.c_str(), desiredAccess, shareMode,
                        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

FileHandle::~FileHandle() {
    if (is_valid()) {
        CloseHandle(handle_);
    }
}

FileHandle::FileHandle(FileHandle&& other) noexcept 
    : handle_(other.handle_), filename_(std::move(other.filename_)) {
    other.handle_ = INVALID_HANDLE_VALUE;
}

FileHandle& FileHandle::operator=(FileHandle&& other) noexcept {
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

} 
