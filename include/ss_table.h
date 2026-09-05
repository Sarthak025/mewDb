#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <map>
#include <optional>
#include "constants.h"

// Format of SS_TABLE
// 
// uint32_t		magic_number;
// uint8_t		version_number;
// uint64_t 	ss_table_index
// uint64_t 	entry_count;
// 
// repeat this for multiple key:val
// 
// operation   uint8_t   (from the shared enum: set / del)
// key_len     uint32_t
// key         char[]    variable
// val_len     uint32_t  (0 for a del entry)
// val         char[]    variable (empty for a del entry)
// 
// uint32_t checksum


enum class lookup_status { 
	not_found, 
	tombstone,
	found
};

struct lookup_result {
    lookup_status status;
    std::optional<std::string> value;  // meaningful only when status == found
};


class ss_table {
private:
	std::string ss_table_file_name;
	uint64_t ss_table_index;
	std::fstream ss_table_file;

public:
	ss_table(uint64_t table_index, open_mode mode);
	~ss_table();

    bool write_to_ss_table(const std::map<std::string, std::optional<std::string>> &mem_table);
	lookup_result get_value_from_ss_table(const std::string &key);

};