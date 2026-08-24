#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>
#include <zlib.h>


bool write_record(std::fstream& file, uint64_t index, uint8_t operation, std::string key, std::string val) {

    uint32_t magic_number = 0xDEADBEEF;
    uint8_t version_number = 1;
    uint32_t key_len = key.length();
    uint32_t val_len = val.length();
    uint32_t crc = crc32(0L, Z_NULL, 0);

    crc = crc32(crc, reinterpret_cast<const Bytef*>(&version_number), sizeof(version_number));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(&index), sizeof(index));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(&operation), sizeof(operation));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(&key_len), sizeof(key_len));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(key.c_str()), key_len);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(&val_len), sizeof(val_len));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(val.c_str()), val_len);


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


