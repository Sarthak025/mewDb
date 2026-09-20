#include "db_engine.h"
#include "wal.h"
#include "constants.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>

struct WalData {
	uint32_t magic_number;
	uint8_t version_number;
	uint64_t index;
	Operation operation;
	uint32_t key_len;
	std::string key;
	uint32_t val_len;
	std::string val;
};

uint32_t calc_checksum(const WalData &record) {
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

Wal::Wal(){
	wal_filename = WAL_FILE_NAME;
	{ std::ofstream create(wal_filename, std::ios::binary | std::ios::app); }
    wal_log_file.open(wal_filename, std::ios::binary | std::ios::in | std::ios::app);
    index = 0;
}

Wal::~Wal(){
    wal_log_file.close();
}

bool Wal::write(Operation operation, const std::string &key, const std::string &val){
    if(operation != Operation::set && operation!= Operation::del) {
        return false;
    }
    
    uint32_t magic_number = WAL_MAGIC_NUMBER;
	uint8_t version_number = WAL_VERSION;
	uint32_t key_len = key.length();
	uint32_t val_len = val.length();

    WalData record = {
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

void Wal::recover(DbEngine& db){

    wal_log_file.seekg(0, std::ios::beg);

	WalData record;
	uint32_t checksum;

	while(true){
		wal_log_file.read(reinterpret_cast<char*>(&record.magic_number), sizeof(record.magic_number));
		if(!wal_log_file || record.magic_number != WAL_MAGIC_NUMBER){
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


		if(record.operation == Operation::set){
			db.recover_set(record.key, record.val);
		}
		else if(record.operation == Operation::del) {
			db.recover_del(record.key);
		}
        index = record.index;
	}

	wal_log_file.clear();
}

bool Wal::truncate() {
	wal_log_file.close();
	{
        std::ofstream truncate_file(wal_filename, std::ios::binary | std::ios::trunc);
        if (!truncate_file) {
            return false;
        }
    }
    wal_log_file.open(wal_filename, std::ios::binary | std::ios::in | std::ios::app);

	return wal_log_file.good();
}