#pragma once

#include <cstdint>
#include <fstream>
#include <string>

class db_engine;

class WAL {
private:
	std::fstream wal_log_file;
	uint64_t index = 0;

public:
	WAL(const std::string& filename);
	~WAL();

	bool write(uint8_t operation, const std::string &key, const std::string &val);
	void recover(db_engine& db);
};
