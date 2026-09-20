#pragma once

#include <cstdint>
#include <string>

enum class OpenMode {
	read,
    write
};

enum class Operation : uint8_t{
    set,
    del
};

// WAL CONSTANTS
constexpr uint8_t WAL_VERSION = 1;
constexpr uint32_t WAL_MAGIC_NUMBER = 0xDEADBEEF;
inline const std::string WAL_FILE_NAME = "data/wal.bin";

// SS_TABLE CONSTSANTS
constexpr uint8_t SS_TABLE_VERSION = 2;
constexpr uint32_t SS_TABLE_MAGIC_NUMBER = 0xDEADBEEF;
inline const std::string SS_TABLE_FILE_NAME = "data/ss_table";
constexpr uint32_t RECORDS_PER_BLOCK = 10;
const uint32_t BYTE_SIZE = 8;


constexpr double BLOOM_FILTER_TARGET_FP_RATE = 0.001;
constexpr uint32_t BLOOM_FILTER_SEED_1 = 6969;
constexpr uint32_t BLOOM_FILTER_SEED_2 = 6767;

// MANIFEST CONSTANTS
inline const std::string MANIFEST_MAGIC_CONST = "MEWDB";
constexpr uint32_t MANIFEST_VERSION = 1;
inline const std::string MANIFEST_FILE_NAME = "data/manifest.txt";
inline const std::string MANIFEST_TEMP_FILE_NAME = "data/temp_manifest.txt";


// DB_ENGINE CONSTANTS
constexpr uint64_t MEM_TABLE_SIZE_LIMIT = 1e3;

