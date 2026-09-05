#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include "constants.h"

class db_engine;

class wal {
private:
	std::string wal_filename;
	std::fstream wal_log_file;
	uint64_t index = 0;

public:
	wal(const std::string& filename);
	~wal();

	bool write(operation operation, const std::string &key, const std::string &val);
	void recover(db_engine& db);
	bool truncate();
};
