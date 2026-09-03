#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <map>
#include <optional>
#include "constants.h"

// Format of SS_TABLE
// 
// uint32_t magic_number;
// uint8_t version_number;
// ss_table_index
// uint64_t entry_count;
// 
// repeat this for multiple key:val
// uint32_t key_len;
// std::string key;
// uint32_t val_len;
// std::string val;
// 
// uint32_t checksum


class ss_table {
private:
	std::string ss_table_file_name;
	uint64_t ss_table_index;
	std::fstream ss_table_file;

public:
	ss_table(const std::string &file_name, uint64_t table_index, open_mode mode);
	~ss_table();

    bool write_to_ss_table(const std::map<std::string, std::string> &mem_table);
	std::optional<std::string> get_value_from_ss_table(const std::string &key);

};