#pragma once

#include <cstdint>
#include <string>

constexpr uint8_t WAL_VERSION = 1;
constexpr uint8_t SS_TABLE_VERSION = 1;

constexpr uint32_t WAL_MAGIC_NUMBER = 0xDEADBEEF;
constexpr uint32_t SS_TABLE_MAGIC_NUMBER = 0xDEADBEEF;

inline const std::string WAL_FILE_NAME = "data/wal";
inline const std::string SS_TABLE_NAME = "data/ss_table";

constexpr uint8_t SET = 0;
constexpr uint8_t DELETE = 1;

constexpr uint64_t MEM_TABLE_SIZE_LIMIT = 1e3;


inline const std::string MANIFEST_MAGIC_CONST = "MEWDB";
constexpr uint32_t MANIFEST_VERSION = 1;
inline const std::string MANIFEST_FILE_NAME = "data/manifest.txt";
inline const std::string MANIFEST_TEMP_FILE_NAME = "data/temp_manifest.txt";
