#pragma once

#include <cstdint>
#include <vector>
#include <string>


class manifest {
private:
    std::string manifest_file_name;
    uint64_t next_ss_table_index;
    std::vector<uint64_t> available_ss_table_indices;

public:
    manifest(const std::string &file_name);
    ~manifest();
    uint64_t get_next_ss_table_index();
    std::vector<uint64_t> get_ss_table_indices();
    bool add_new_ss_table_index(uint64_t index);
};