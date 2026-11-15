#pragma once

#include <cstdint>
#include <filesystem>


std::filesystem::path wal_file_path(const std::filesystem::path &data_dir);

std::filesystem::path manifest_file_path(const std::filesystem::path &data_dir);

std::filesystem::path temp_manifest_file_path(const std::filesystem::path &data_dir);

std::filesystem::path ss_table_file_path(const std::filesystem::path &data_dir, uint64_t index);
