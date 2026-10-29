#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include "constants.h"

class DbEngine;

class Wal {
private:
	std::string wal_filename;
	std::unique_ptr<std::FILE, decltype(&std::fclose)> wal_log_file{nullptr, &std::fclose};
	uint64_t index = 0;
	bool write_failed = false;

public:
	Wal();

	bool write(Operation operation, const std::string &key, const std::string &val);
	void recover(DbEngine& db);
	bool truncate();
};
