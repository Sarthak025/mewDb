#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

class wal;

class db_engine {
private:
	friend class wal;
	std::map<std::string, std::string> storage;
	uint64_t mem_table_size = 0; //This is just the length of all the keys and vals
	wal *wal_instance;
	uint64_t ss_table_index = 0;
	void recover_set(const std::string &key, const std::string &val);
	void recover_del(const std::string &key);

public:
	db_engine(const std::string &wal_filename);
	~db_engine();
	void set(const std::string &key, const std::string &val);
	std::optional<std::string> get(const std::string &key);
	bool del(const std::string &key);
	bool exists(const std::string &key);
	std::vector<std::string> keys();
	std::vector<std::pair<std::string, std::string>> range(const std::string &start, const std::string &end);
	std::vector<std::pair<std::string, std::string>> prefix_scan(const std::string &prefix);
	void flush();
};
