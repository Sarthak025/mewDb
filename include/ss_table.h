#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <map>

class ss_table {
private:
	std::string ss_table_file_name;
	uint64_t ss_table_index;
	std::fstream ss_table_file;

public:
	ss_table(const std::string &file_name, uint64_t table_index);
	~ss_table();

    bool write_to_ss_table(const std::map<std::string, std::string> &mem_table);

};