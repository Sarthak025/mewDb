#include "db_engine.h"
#include "wal.h"
#include "constants.h"
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <zlib.h>

#if defined(_WIN32)
#include <io.h>
#elif defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#elif defined(__unix__)
#include <unistd.h>
#else
#error "WAL syncing is not implemented for this platform"
#endif

namespace {

bool sync_file(std::FILE *file) {
	if (std::ferror(file) || std::fflush(file) != 0) {
		return false;
	}

#if defined(_WIN32)
	const int fd = ::_fileno(file);
	return fd != -1 && ::_commit(fd) == 0;
#else
	const int fd = ::fileno(file);
	if (fd == -1) {
		return false;
	}

	int result;
	do {
#if defined(__APPLE__)
		result = ::fcntl(fd, F_FULLFSYNC);
#else
		result = ::fsync(fd);
#endif
	} while (result == -1 && errno == EINTR);
	return result == 0;
#endif
}

bool write_bytes(std::FILE *file, const void *data, std::size_t size) {
	return size == 0 || std::fwrite(data, 1, size, file) == size;
}

bool read_bytes(std::FILE *file, void *data, std::size_t size) {
	return size == 0 || std::fread(data, 1, size, file) == size;
}

#if defined(_WIN32)
using FileOffset = std::int64_t;
#else
using FileOffset = off_t;
#endif

bool seek_file(std::FILE *file, FileOffset offset, int origin) {
#if defined(_WIN32)
	return ::_fseeki64(file, offset, origin) == 0;
#else
	return ::fseeko(file, offset, origin) == 0;
#endif
}

FileOffset file_position(std::FILE *file) {
#if defined(_WIN32)
	const FileOffset position = ::_ftelli64(file);
#else
	const FileOffset position = ::ftello(file);
#endif
	if (position == -1) {
		throw std::system_error(errno, std::generic_category(), "Cannot determine WAL position");
	}
	return position;
}

bool truncate_file(std::FILE *file, FileOffset length) {
#if defined(_WIN32)
	const int fd = ::_fileno(file);
	return fd != -1 && ::_chsize_s(fd, length) == 0;
#else
	const int fd = ::fileno(file);
	if (fd == -1) {
		return false;
	}
	int result;
	do {
		result = ::ftruncate(fd, length);
	} while (result == -1 && errno == EINTR);
	return result == 0;
#endif
}

bool read_string(std::FILE *file, std::string &value, uint32_t length, FileOffset file_size) {
	const FileOffset position = file_position(file);
	// A corrupt length must not allocate more memory than the file can contain.
	if (position > file_size || length > static_cast<std::uint64_t>(file_size - position)) {
		return false;
	}
	value.resize(length);
	return read_bytes(file, value.data(), length);
}

enum class StopReason { torn_tail, corrupt_record };

} // namespace

struct WalData {
	uint32_t magic_number;
	uint8_t version_number;
	uint64_t index;
	Operation operation;
	uint32_t key_len;
	uint32_t val_len;
	std::string key;
	std::string val;
};

uint32_t calc_header_checksum(const WalData &record) {
	uint32_t crc = crc32(0L, Z_NULL, 0);

	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.version_number), sizeof(record.version_number));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.index), sizeof(record.index));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.operation), sizeof(record.operation));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.key_len), sizeof(record.key_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.val_len), sizeof(record.val_len));

	return crc;
}

uint32_t calc_record_checksum(const WalData &record, uint32_t header_checksum) {
	uint32_t crc = crc32(header_checksum, reinterpret_cast<const Bytef *>(record.key.c_str()), record.key_len);
	crc = crc32(crc, reinterpret_cast<const Bytef *>(record.val.c_str()), record.val_len);

	return crc;
}

Wal::Wal() : wal_filename(WAL_FILE_NAME) {
	wal_log_file.reset(std::fopen(wal_filename.c_str(), "a+b"));
	if (!wal_log_file) {
		throw std::system_error(errno, std::generic_category(), "Cannot open WAL");
	}
}

bool Wal::write(Operation operation, const std::string &key, const std::string &val){
    if(operation != Operation::set && operation!= Operation::del) {
		return false;
    }

	if (!wal_log_file || write_failed ||
		key.size() > std::numeric_limits<uint32_t>::max() ||
		val.size() > std::numeric_limits<uint32_t>::max()) {
		return false;
	}
    
    uint32_t magic_number = WAL_MAGIC_NUMBER;
	uint8_t version_number = WAL_VERSION;
	uint32_t key_len = static_cast<uint32_t>(key.size());
	uint32_t val_len = static_cast<uint32_t>(val.size());

    WalData record = {
        magic_number,
        version_number,
        index,
        operation,
        key_len,
        val_len,
        key,
        val
    };

	uint32_t header_crc = calc_header_checksum(record);
	uint32_t record_crc = calc_record_checksum(record, header_crc);

	std::FILE *file = wal_log_file.get();
	if (!write_bytes(file, &magic_number, sizeof(magic_number)) ||
		!write_bytes(file, &version_number, sizeof(version_number)) ||
		!write_bytes(file, &index, sizeof(index)) ||
		!write_bytes(file, &operation, sizeof(operation)) ||
		!write_bytes(file, &key_len, sizeof(key_len)) ||
		!write_bytes(file, &val_len, sizeof(val_len)) ||
		!write_bytes(file, &header_crc, sizeof(header_crc)) ||
		!write_bytes(file, key.data(), key_len) ||
		!write_bytes(file, val.data(), val_len) ||
		!write_bytes(file, &record_crc, sizeof(record_crc)) ||
		!sync_file(file)) {
		
		write_failed = true;
		return false;
	}
	index++;
	return true;
}

void Wal::recover(DbEngine& db){

	if (!wal_log_file || write_failed) {
		throw std::runtime_error("Cannot recover an unavailable or failed WAL");
	}

	write_failed = true;
	std::FILE *file = wal_log_file.get();
	if (!seek_file(file, 0, SEEK_END)) {
		throw std::runtime_error("Cannot seek WAL to determine its size");
	}
	const FileOffset file_size = file_position(file);
	if (!seek_file(file, 0, SEEK_SET)) {
		throw std::runtime_error("Cannot seek WAL for recovery");
	}
	FileOffset last_valid_end = 0;

	WalData record{};
	uint32_t header_checksum = 0;
	uint32_t record_checksum = 0;

	StopReason stop_reason = StopReason::torn_tail;

	while(true){
		if (!read_bytes(file, &record.magic_number, sizeof(record.magic_number))) {
			break;
		}
		if (record.magic_number != WAL_MAGIC_NUMBER) {
			stop_reason = StopReason::corrupt_record;
			break;
		}

		if (!read_bytes(file, &record.version_number, sizeof(record.version_number))) {
			break;
		}
		if (record.version_number != WAL_VERSION) {
			stop_reason = StopReason::corrupt_record;
			break;
		}
		if (!read_bytes(file, &record.index, sizeof(record.index)) ||
			!read_bytes(file, &record.operation, sizeof(record.operation))) {
			break;
		}
		if (record.operation != Operation::set && record.operation != Operation::del) {
			stop_reason = StopReason::corrupt_record;
			break;
		}

		//Read lengths and header checksum
		if (!read_bytes(file, &record.key_len, sizeof(record.key_len)) ||
			!read_bytes(file, &record.val_len, sizeof(record.val_len)) ||
			!read_bytes(file, &header_checksum, sizeof(header_checksum))) {
			break;
		}
		// Once lengths are validated, missing bytes indicate a torn write.
		if (calc_header_checksum(record) != header_checksum) {
			stop_reason = StopReason::corrupt_record;
			break;
		}

		//Read key and value
		if (!read_string(file, record.key, record.key_len, file_size) ||
			!read_string(file, record.val, record.val_len, file_size)) {
			break;
		}

		//Read the record checksum
		if (!read_bytes(file, &record_checksum, sizeof(record_checksum))) {
			break;
		}
		if (calc_record_checksum(record, header_checksum) != record_checksum) {
			stop_reason = StopReason::corrupt_record;
			break;
		}

		// Only a complete record with a matching checksum extends the valid prefix.
		last_valid_end = file_position(file);

		if(record.operation == Operation::set){
			db.recover_set(record.key, record.val);
		}
		else if(record.operation == Operation::del) {
			db.recover_del(record.key);
		}
        index = record.index + 1;
	}

	if (std::ferror(file)) {
		throw std::runtime_error("Cannot read WAL during recovery");
	}
	std::clearerr(file);

	// Leave input mode before changing the file through its descriptor.
	if (!seek_file(file, 0, SEEK_END)) {
		throw std::runtime_error("Cannot seek WAL for appending");
	}
	if (last_valid_end < file_size) {
		if (!truncate_file(file, last_valid_end)) {
			throw std::system_error(errno, std::generic_category(), "Cannot truncate invalid WAL tail");
		}
		if (!seek_file(file, last_valid_end, SEEK_SET)) {
			throw std::runtime_error("Cannot seek WAL after tail repair");
		}
		if (!sync_file(file)) {
			throw std::system_error(errno, std::generic_category(), "Cannot sync repaired WAL");
		}

		const std::string description = stop_reason == StopReason::torn_tail
			? "torn tail (expected after a crash)"
			: "corrupt record (possible data loss)";

		std::cerr << "WAL recovery: discarded " << (file_size - last_valid_end)
			<< " bytes at offset " << last_valid_end << ": " << description << '\n';
	}
	write_failed = false;
}

bool Wal::truncate() {
	write_failed = true;
	std::FILE *file = wal_log_file.release();
	if (file && std::fclose(file) != 0) {
		return false;
	}
	wal_log_file.reset(std::fopen(wal_filename.c_str(), "w+b"));
	if (!wal_log_file || !sync_file(wal_log_file.get())) {
		return false;
	}
	file = wal_log_file.release();
	if (std::fclose(file) != 0) {
		return false;
	}
	wal_log_file.reset(std::fopen(wal_filename.c_str(), "a+b"));
	write_failed = !wal_log_file;
	return !write_failed;
}