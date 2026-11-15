#include "file_names.h"
#include "constants.h"

#include <string>

// <data_dir>/wal.bin
std::filesystem::path wal_file_path(const std::filesystem::path &data_dir) {
	return data_dir / WAL_FILE_NAME;
}

// <data_dir>/manifest.txt
std::filesystem::path manifest_file_path(const std::filesystem::path &data_dir) {
	return data_dir / MANIFEST_FILE_NAME;
}

// <data_dir>/temp_manifest.txt
std::filesystem::path temp_manifest_file_path(const std::filesystem::path &data_dir) {
	return data_dir / MANIFEST_TEMP_FILE_NAME;
}

// <data_dir>/ss_table_<index>.bin
std::filesystem::path ss_table_file_path(const std::filesystem::path &data_dir, uint64_t index) {
	return data_dir / (SS_TABLE_FILE_NAME + "_" + std::to_string(index) + ".bin");
}
