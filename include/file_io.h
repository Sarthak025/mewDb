#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#if defined(_WIN32)
using FileOffset = std::int64_t;
#else
#include <sys/types.h>
using FileOffset = off_t;
#endif

enum class WalSyncLevel {
	process_crash,  // fflush: in the OS, survives the process dying
	os_crash,       // + fsync: handed to the drive, survives the OS crashing
	power_loss      // + F_FULLFSYNC on macOS (fsync elsewhere): past the drive's cache, survives power loss
};

bool write_bytes(std::FILE *file, const void *data, std::size_t size);

FileOffset file_position(std::FILE *file);

bool sync_file(std::FILE *file, WalSyncLevel level);

bool sync_parent_directory(const std::string &file_path);
