#ifndef REGION_OPS_HPP
#define REGION_OPS_HPP

#include <vector>
#include <cstdint>
#include <string>
#include "file_handle.hpp"
#include "file_lock.hpp"
#include "ipc_common.hpp"

namespace FileSocket {

std::string get_last_windows_error();
bool write_to_region_impl(const std::string& filename, uint32_t offset, const std::vector<char>& data);
std::vector<char> read_from_region_impl(const std::string& filename, uint32_t offset, uint32_t size);

} 

#endif