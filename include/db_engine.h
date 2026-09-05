#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

class wal;
class manifest;

class db_engine {
private:
	friend class wal;
	std::map<std::string, std::optional<std::string>> curr_mem_table;
	uint64_t mem_table_size = 0; //This is just the length of all the keys and vals
	wal *wal_instance;
	manifest *manifest_instance;
	void recover_set(const std::string &key, const std::string &val);
	void recover_del(const std::string &key);

public:
	db_engine();
	~db_engine();
	void set(const std::string &key, const std::string &val);
	std::optional<std::string> get(const std::string &key);
	bool del(const std::string &key);
	bool exists_in_curr_mem_table(const std::string &key);
	std::vector<std::string> keys();
	std::vector<std::pair<std::string, std::optional<std::string>>> range(const std::string &start, const std::string &end);
	std::vector<std::pair<std::string, std::optional<std::string>>> prefix_scan(const std::string &prefix);
	bool flush();
};
