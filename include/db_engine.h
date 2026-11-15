#pragma once

#include "file_io.h"
#include "constants.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

class Wal;
class Manifest;

struct Options {
	std::filesystem::path data_dir = "data";
	uint64_t mem_table_size_limit = DEFAULT_MEM_TABLE_SIZE_LIMIT;
	WalSyncLevel wal_sync = WalSyncLevel::power_loss;
	bool create_if_missing = false;
};


class DbEngine {
private:
	friend class Wal;

	Options options;
	std::map<std::string, std::optional<std::string>> curr_mem_table;
	uint64_t mem_table_size = 0; //This is just the length of all the keys and vals
	std::unique_ptr<Wal> wal_instance;
	std::unique_ptr<Manifest> manifest_instance;
	void recover_set(const std::string &key, const std::string &val);
	void recover_del(const std::string &key);
	bool exists_in_curr_mem_table(const std::string &key);
	void check_mem_table_size_to_flush();
	
public:
	explicit DbEngine(Options options = {});
	~DbEngine();
	void set(const std::string &key, const std::string &val);
	std::optional<std::string> get(const std::string &key);
	bool del(const std::string &key);
	std::vector<std::string> keys();
	bool exists(const std::string &key);
	std::vector<std::pair<std::string, std::optional<std::string>>> range(const std::string &start, const std::string &end);
	std::vector<std::pair<std::string, std::optional<std::string>>> prefix_scan(const std::string &prefix);
	bool compact();
	bool flush();
};
