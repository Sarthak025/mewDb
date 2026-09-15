#pragma once

#include <cstdint>
#include <string>

enum class open_mode {
	read,
    write
};

enum class operation : uint8_t{
    set,
    del
};

// WAL CONSTANTS
constexpr uint8_t WAL_VERSION = 1;
constexpr uint32_t WAL_MAGIC_NUMBER = 0xDEADBEEF;
inline const std::string WAL_FILE_NAME = "data/wal.bin";

// SS_TABLE CONSTSANTS
constexpr uint8_t SS_TABLE_VERSION = 1;
constexpr uint32_t SS_TABLE_MAGIC_NUMBER = 0xDEADBEEF;
inline const std::string SS_TABLE_FILE_NAME = "data/ss_table";

// MANIFEST CONSTANTS
inline const std::string MANIFEST_MAGIC_CONST = "MEWDB";
constexpr uint32_t MANIFEST_VERSION = 1;
inline const std::string MANIFEST_FILE_NAME = "data/manifest.txt";
inline const std::string MANIFEST_TEMP_FILE_NAME = "data/temp_manifest.txt";


// DB_ENGINE CONSTANTS
constexpr uint64_t MEM_TABLE_SIZE_LIMIT = 1e3;