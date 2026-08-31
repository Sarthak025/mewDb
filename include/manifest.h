#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>

// Format for manifest file
// 
// MANIFEST_MAGIC_CONST
// MANIFEST_VERSION
// NEXT_SS_TABLE_INDEX
// NUM_SS_TABLES
// SS_TABLE_INDEX_1
// SS_TABLE_INDEX_2
// ......

class manifest {
private:
    std::string manifest_file_name;
    std::fstream manifest_file;
    uint32_t manifest_version;
    uint64_t next_ss_table_index;
    uint64_t num_ss_tables;

public:
    manifest(const std::string &file_name);
    ~manifest();
    std::vector<uint64_t> get_ss_table_indices();
    uint64_t get_next_ss_table_index();
    bool add_new_ss_table_index(uint64_t index);
};