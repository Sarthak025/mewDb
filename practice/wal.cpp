#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>

// WAL FORMAT
//
// [magic_number]   - uint32_t - 4 bytes
// [version_number] - uint8_t   - 1 bytes
// [index]          - uint64_t - 8 bytes
// [operation]      - uint8_t  - 1 byte
// [len_of_key]     - uint32_t - 4 bytes
// [val_of_key]     - char[]   - len_of_key bytes
// [len_of_data]    - uint32_t - 4 bytes
// [val_of_data]    - char[]   - len_of_data bytes
// [checksum]       - uint32_t   - 4 bytes

constexpr uint8_t SET = 0;
constexpr uint8_t DELETE = 1;
constexpr uint32_t MAGIC_NUMBER = 0xDEADBEEF;

struct wal_data {
	uint32_t magic_number;
	uint8_t version_number;
	uint64_t index;
	uint8_t operation;
	uint32_t key_len;
	std::string key;
	uint32_t val_len;
	std::string val;
};

uint32_t calc_checksum(const wal_data &record) {
	uint32_t crc = crc32(0L, Z_NULL, 0);

	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.version_number), sizeof(record.version_number));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.index), sizeof(record.index));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.operation), sizeof(record.operation));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.key_len), sizeof(record.key_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(record.key.c_str()), record.key_len);
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&record.val_len), sizeof(record.val_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(record.val.c_str()), record.val_len);

	return crc;
}

bool write_record_v1(std::fstream &file, uint64_t index, uint8_t operation, std::string key, std::string val) {

	uint32_t magic_number = MAGIC_NUMBER;
	uint8_t version_number = 1;
	uint32_t key_len = key.length();
	uint32_t val_len = val.length();

    wal_data record = {
        magic_number,
        version_number,
        index,
        operation,
        key_len,
        key, 
        val_len,
        val
    };

	uint32_t crc = calc_checksum(record);

	file.write(reinterpret_cast<char *>(&magic_number), sizeof(magic_number));
	file.write(reinterpret_cast<char *>(&version_number), sizeof(version_number));
	file.write(reinterpret_cast<char *>(&index), sizeof(index));
	file.write(reinterpret_cast<char *>(&operation), sizeof(operation));
	file.write(reinterpret_cast<char *>(&key_len), sizeof(key_len));
	file.write(key.c_str(), key_len);
	file.write(reinterpret_cast<char *>(&val_len), sizeof(val_len));
	file.write(val.c_str(), val_len);
	file.write(reinterpret_cast<char *>(&crc), sizeof(crc));

	return file.good();
}

void recover(std::fstream &file) {
	
	file.seekg(0, std::ios::beg);

	wal_data record;
	uint32_t checksum;

	while(true){
		file.read(reinterpret_cast<char*>(&record.magic_number), sizeof(record.magic_number));
		if(!file || record.magic_number != MAGIC_NUMBER){
			break;
		}

		file.read(reinterpret_cast<char*>(&record.version_number), sizeof(record.version_number));
		file.read(reinterpret_cast<char*>(&record.index), sizeof(record.index));
		file.read(reinterpret_cast<char*>(&record.operation), sizeof(record.operation));


		//Read key
		file.read(reinterpret_cast<char*>(&record.key_len), sizeof(record.key_len));
		std::string key(record.key_len, '\0');
		file.read(reinterpret_cast<char*>(key.data()), record.key_len);
		record.key = key;
		
		//Read Value
		file.read(reinterpret_cast<char*>(&record.val_len), sizeof(record.val_len));
		std::string val(record.val_len, '\0');
		file.read(reinterpret_cast<char*>(val.data()), record.val_len);
		record.val = val;

		//Read the checksum
		file.read(reinterpret_cast<char*>(&checksum), sizeof(checksum));
		uint32_t new_checksum = calc_checksum(record);
		if(new_checksum != checksum){
			std::cout << "Checksum not matched" << '\n';
			break;
		}


		if(record.operation == SET){
			std::cout << "SET : " << key << " : " << val <<'\n';
		}
		else if(record.operation == DELETE) {
			std::cout << "DELETE : " << key << '\n';
		}
	}

}

int main() {

	std::fstream file("wal.bin", std::ios::binary | std::ios::out | std::ios::app);

	// write
	if (!write_record_v1(file, 1, SET, "user:1", "alice")) {
		std::cout << "failed 1";
	} else
		std::cout << "write 1 OK\n";
	if (!write_record_v1(file, 2, DELETE, "user:2", "")) {
		std::cout << "failed 2";
	} else
		std::cout << "write 2 OK\n";

	file.close();


	std::fstream file2("wal.bin", std::ios::binary | std::ios::in);
	recover(file2);
}
