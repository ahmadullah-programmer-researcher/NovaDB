#pragma once
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>

namespace novadb::codec {
void write_u32(std::ostream& out, std::uint32_t value);
void write_u64(std::ostream& out, std::uint64_t value);
bool read_u32(std::istream& in, std::uint32_t& value);
bool read_u64(std::istream& in, std::uint64_t& value);
void write_string(std::ostream& out, const std::string& value);
bool read_string(std::istream& in, std::string& value, std::uint32_t max_size = 64 * 1024 * 1024);
std::uint32_t checksum(const std::string& bytes);
}
