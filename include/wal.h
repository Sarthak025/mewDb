#pragma once

#include "constants.h"
#include "file_io.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <filesystem>

class DbEngine;

class Wal {
private:
	std::string wal_filename;
	WalSyncLevel sync_level; // per-record sync in write(); recovery and truncate() always sync fully
	std::unique_ptr<std::FILE, decltype(&std::fclose)> wal_log_file{nullptr, &std::fclose};
	uint64_t index = 0;
	bool write_failed = false;

public:
	explicit Wal(std::filesystem::path wal_path, WalSyncLevel wal_sync);

	bool write(Operation operation, const std::string &key, const std::string &val);
	void recover(DbEngine& db);
	bool truncate();
};
