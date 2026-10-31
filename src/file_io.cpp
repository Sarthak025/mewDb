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

bool write_bytes(std::FILE *file, const void *data, std::size_t size) {
	return size == 0 || std::fwrite(data, 1, size, file) == size;
}

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
