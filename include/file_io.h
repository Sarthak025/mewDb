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

bool write_bytes(std::FILE *file, const void *data, std::size_t size);

FileOffset file_position(std::FILE *file);

bool sync_file(std::FILE *file);

bool sync_parent_directory(const std::string &file_path);
