#pragma once

#include <cstdint>
#include <string>

constexpr uint8_t VERSION = 1;
constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF;
constexpr uint8_t SET = 0;
constexpr uint8_t DELETE = 1;


constexpr uint64_t MEM_TABLE_SIZE_LIMIT = 1e3;
inline const std::string SS_TABLE_NAME = "ss_table";