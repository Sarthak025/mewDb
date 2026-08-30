#include "db_engine.h"
#include "wal.h"
#include "ss_table.h"
#include "constants.h"

#include <string>
#include <cstdint>
#include <zlib.h>

uint32_t key_val_checksum(uint32_t crc, const std::string &key, const std::string &val) {
    uint32_t key_len = key.length();
	uint32_t val_len = val.length();

	crc = crc32(crc, reinterpret_cast<const Bytef *>(&key_len), sizeof(key_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(key.c_str()), key_len);
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&val_len), sizeof(val_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(val.c_str()), val_len);

	return crc;
}



ss_table::ss_table(const std::string &file_name, uint64_t table_index){
    ss_table_file_name = file_name + "_" + std::to_string(table_index) + ".bin";
    ss_table_index = table_index;

    { std::ofstream create(ss_table_file_name, std::ios::binary | std::ios::app); }
    ss_table_file.open(ss_table_file_name, std::ios::binary | std::ios::out | std::ios::app);
}

ss_table::~ss_table(){
    ss_table_file.close();
}

bool ss_table::write_to_ss_table(const std::map<std::string, std::string> &mem_table){
    uint32_t magic_number = SS_TABLE_MAGIC_NUMBER;
    uint8_t version = SS_TABLE_VERSION;
    // ss_table_index
    uint64_t entry_count = static_cast<uint64_t>(mem_table.size());

    //start calculating checksum
    uint32_t crc = crc32(0L, Z_NULL, 0);

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&version), sizeof(version));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&ss_table_index), sizeof(ss_table_index));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&entry_count), sizeof(entry_count));

    //start writing in ss_table
    ss_table_file.write(reinterpret_cast<char *>(&magic_number), sizeof(magic_number));
	ss_table_file.write(reinterpret_cast<char *>(&version), sizeof(version));
	ss_table_file.write(reinterpret_cast<char *>(&ss_table_index), sizeof(ss_table_index));
	ss_table_file.write(reinterpret_cast<char *>(&entry_count), sizeof(entry_count));

    for(const auto &[key, val] : mem_table){
        uint32_t key_len = key.length();
	    uint32_t val_len = val.length();

        ss_table_file.write(reinterpret_cast<char *>(&key_len), sizeof(key_len));
        ss_table_file.write(key.c_str(), key_len);
        ss_table_file.write(reinterpret_cast<char *>(&val_len), sizeof(val_len));
        ss_table_file.write(val.c_str(), val_len);

        crc = key_val_checksum(crc, key, val);
        
    }

    ss_table_file.write(reinterpret_cast<char *>(&crc), sizeof(crc));

    return ss_table_file.good();

}