#include "db_engine.h"
#include "wal.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>

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

wal::wal(const std::string& filename){
	{ std::ofstream create(filename, std::ios::binary | std::ios::app); }
    wal_log_file.open(filename, std::ios::binary | std::ios::in | std::ios::app);
    index = 0;
}

wal::~wal(){
    wal_log_file.close();
}

bool wal::write(uint8_t operation, const std::string &key, const std::string &val){
    if(operation != SET && operation!= DELETE) {
        return false;
    }
    
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

	wal_log_file.write(reinterpret_cast<char *>(&magic_number), sizeof(magic_number));
	wal_log_file.write(reinterpret_cast<char *>(&version_number), sizeof(version_number));
	wal_log_file.write(reinterpret_cast<char *>(&index), sizeof(index));
	wal_log_file.write(reinterpret_cast<char *>(&operation), sizeof(operation));
	wal_log_file.write(reinterpret_cast<char *>(&key_len), sizeof(key_len));
	wal_log_file.write(key.c_str(), key_len);
	wal_log_file.write(reinterpret_cast<char *>(&val_len), sizeof(val_len));
	wal_log_file.write(val.c_str(), val_len);
	wal_log_file.write(reinterpret_cast<char *>(&crc), sizeof(crc));


    if(wal_log_file.good()){
		index++;
		return true;
	}
	else{
		return false;
	}
}

void wal::recover(db_engine& db){

    wal_log_file.seekg(0, std::ios::beg);

	wal_data record;
	uint32_t checksum;

	while(true){
		wal_log_file.read(reinterpret_cast<char*>(&record.magic_number), sizeof(record.magic_number));
		if(!wal_log_file || record.magic_number != MAGIC_NUMBER){
			break;
		}

		wal_log_file.read(reinterpret_cast<char*>(&record.version_number), sizeof(record.version_number));
		wal_log_file.read(reinterpret_cast<char*>(&record.index), sizeof(record.index));
		wal_log_file.read(reinterpret_cast<char*>(&record.operation), sizeof(record.operation));

		//Read key
		wal_log_file.read(reinterpret_cast<char*>(&record.key_len), sizeof(record.key_len));
		record.key.resize(record.key_len);
		wal_log_file.read(reinterpret_cast<char*>(record.key.data()), record.key_len);
		
		//Read Value
		wal_log_file.read(reinterpret_cast<char*>(&record.val_len), sizeof(record.val_len));
		record.val.resize(record.val_len);
		wal_log_file.read(reinterpret_cast<char*>(record.val.data()), record.val_len);

		//Read the checksum
		wal_log_file.read(reinterpret_cast<char*>(&checksum), sizeof(checksum));
		uint32_t new_checksum = calc_checksum(record);
		if(new_checksum != checksum){
			std::cout << "Recovery stopped" << '\n';
			break;
		}


		if(record.operation == SET){
			db.recover_set(record.key, record.val);
		}
		else if(record.operation == DELETE) {
			db.recover_del(record.key);
		}
        index = record.index;
	}

	wal_log_file.clear();
}