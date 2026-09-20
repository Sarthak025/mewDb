#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include "constants.h"

class DbEngine;

class Wal {
private:
	std::string wal_filename;
	std::fstream wal_log_file;
	uint64_t index = 0;

public:
	Wal();
	~Wal();

	bool write(Operation operation, const std::string &key, const std::string &val);
	void recover(DbEngine& db);
	bool truncate();
};
