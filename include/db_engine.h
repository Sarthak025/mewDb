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
	wal *wal_instance;
	void recover_set(const std::string &key, const std::string &val);
	void recover_del(const std::string &key);

public:
	db_engine(const std::string &wal_filename);
	~db_engine();
	void set(const std::string &key, const std::string &val);
	std::optional<std::string> get(const std::string &key);
	int del(const std::string &key);
	bool exists(const std::string &key);
	std::vector<std::string> keys();
	std::vector<std::pair<std::string, std::string>>
	range(const std::string &start, const std::string &end);
	std::vector<std::pair<std::string, std::string>>
	prefix_scan(const std::string &prefix);
};
