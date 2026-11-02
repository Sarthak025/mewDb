#include "file_io.h"
#include <cerrno>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#include <io.h>
#elif defined(__APPLE__) || defined(__unix__)
#include <fcntl.h>
#include <unistd.h>
#else
#error "File syncing is not implemented for this platform"
#endif

namespace {

#if !defined(_WIN32)
// Forces fd's data to stable storage: F_FULLFSYNC on macOS (also empties the drive's own cache),
// fsync elsewhere. Retries if a signal interrupts it (EINTR). True on success.
bool sync_descriptor(int fd) {
	int result;
	do {
#if defined(__APPLE__)
		result = ::fcntl(fd, F_FULLFSYNC);
#else
		result = ::fsync(fd);
#endif
	} while (result == -1 && errno == EINTR);
	return result == 0;
}
#endif

} // namespace

// Writes size bytes from data into file's stdio buffer. True only if every byte was accepted.
// Does not flush or sync; sync_file() does that.
bool write_bytes(std::FILE *file, const void *data, std::size_t size) {
	return size == 0 || std::fwrite(data, 1, size, file) == size;
}

// Returns file's current byte offset (64-bit safe). Throws std::system_error if it can't be determined.
FileOffset file_position(std::FILE *file) {
#if defined(_WIN32)
	const FileOffset position = ::_ftelli64(file);
#else
	const FileOffset position = ::ftello(file);
#endif
	if (position == -1) {
		throw std::system_error(errno, std::generic_category(), "Cannot determine file position");
	}
	return position;
}

// Pushes file's stdio buffer to the OS (fflush), then forces it to stable storage.
// False if the stream already had an error or either step fails.
bool sync_file(std::FILE *file) {
	if (std::ferror(file) || std::fflush(file) != 0) {
		return false;
	}

#if defined(_WIN32)
	const int fd = ::_fileno(file);
	return fd != -1 && ::_commit(fd) == 0;
#else
	const int fd = ::fileno(file);
	return fd != -1 && sync_descriptor(fd);
#endif
}

// Syncs the directory that contains file_path (the current directory if it has none), so a file created,
// renamed or deleted in it survives a power loss. On failure errno comes from the sync, not the close.
// Does nothing and returns true on Windows.
bool sync_parent_directory(const std::string &file_path) {
#if defined(_WIN32)
	(void)file_path;
	return true;
#else
	std::filesystem::path directory = std::filesystem::path(file_path).parent_path();
	if (directory.empty()) {
		directory = ".";
	}

	// A directory has no FILE* to fflush; sync it through a plain read-only descriptor.
	int fd;
	do {
		fd = ::open(directory.c_str(), O_RDONLY);
	} while (fd == -1 && errno == EINTR);
	if (fd == -1) {
		return false;
	}

	const bool synced = sync_descriptor(fd);
	const int sync_errno = errno;
	::close(fd);
	errno = sync_errno;
	return synced;
#endif
}
