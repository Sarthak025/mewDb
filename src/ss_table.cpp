#include "db_engine.h"
#include "wal.h"
#include "ss_table.h"
#include "constants.h"
#include "manifest.h"
#include <string>
#include <cstdint>
#include <zlib.h>
#include <utility>

using Record = std::pair<std::string, std::optional<std::string>>;

uint32_t key_val_checksum(operation op, uint32_t crc, const std::string &key, const std::string &val) {
    uint32_t key_len = key.length();
	uint32_t val_len = val.length();

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&op), sizeof(op));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&key_len), sizeof(key_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(key.c_str()), key_len);
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&val_len), sizeof(val_len));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(val.c_str()), val_len);

	return crc;
}

void write_header_block(std::fstream &ss_table_file, uint64_t ss_table_index, std::uint64_t total_entry_cnt){
    uint32_t magic_number = SS_TABLE_MAGIC_NUMBER;
    uint8_t version = SS_TABLE_VERSION;
    // ss_table_index

    //start calculating checksum
    uint32_t crc = crc32(0L, Z_NULL, 0);

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&version), sizeof(version));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&ss_table_index), sizeof(ss_table_index));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&total_entry_cnt), sizeof(total_entry_cnt));

    //start writing in header
    ss_table_file.write(reinterpret_cast<char *>(&magic_number), sizeof(magic_number));
	ss_table_file.write(reinterpret_cast<char *>(&version), sizeof(version));
	ss_table_file.write(reinterpret_cast<char *>(&ss_table_index), sizeof(ss_table_index));
	ss_table_file.write(reinterpret_cast<char *>(&total_entry_cnt), sizeof(total_entry_cnt));
    ss_table_file.write(reinterpret_cast<char *>(&crc), sizeof(crc));

}

void write_data_block(std::fstream &ss_table_file, std::vector<Record> &data){
    uint64_t entry_cnt = data.size();
    
    //start calculating checksum
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&entry_cnt), sizeof(entry_cnt));
    ss_table_file.write(reinterpret_cast<char *>(&entry_cnt),sizeof(entry_cnt));

    for(const auto &[key, val] : data){
        operation op = (val.has_value()) ? operation::set : operation::del;

        uint32_t key_len = key.length();
	    uint32_t val_len = (val.has_value()) ? val.value().length() : 0;

        ss_table_file.write(reinterpret_cast<char *>(&op),sizeof(op));
        ss_table_file.write(reinterpret_cast<char *>(&key_len), sizeof(key_len));
        ss_table_file.write(key.c_str(), key_len);
        ss_table_file.write(reinterpret_cast<char *>(&val_len), sizeof(val_len));

        if (val.has_value()) {
            ss_table_file.write((val.value()).c_str(), val_len);
            crc = key_val_checksum(op, crc, key, val.value());
        } else {
            crc = key_val_checksum(op, crc, key, "");
        }
    }
    ss_table_file.write(reinterpret_cast<char *>(&crc), sizeof(crc));

}


// TODO: need to change this structure
ss_table_data ss_table::read_ss_table() {

    ss_table_data curr_data;

    // Move to begining to the file
    ss_table_file.seekg(0, std::ios::beg);

    //start calculating checksum
    uint32_t crc = crc32(0L, Z_NULL, 0);

    ss_table_file.read(reinterpret_cast<char *>(&curr_data.magic_num), sizeof(curr_data.magic_num));

    if(curr_data.magic_num != SS_TABLE_MAGIC_NUMBER){
        throw std::runtime_error("corrupted ss_table...");
    }

	ss_table_file.read(reinterpret_cast<char *>(&curr_data.version_num), sizeof(curr_data.version_num));
	ss_table_file.read(reinterpret_cast<char *>(&curr_data.ss_table_idx), sizeof(curr_data.ss_table_idx));
    if (curr_data.ss_table_idx != ss_table_index) {
        throw std::runtime_error("ss_table index mismatch...");
    }
	ss_table_file.read(reinterpret_cast<char *>(&curr_data.entry_cnt), sizeof(curr_data.entry_cnt));

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&curr_data.version_num), sizeof(curr_data.version_num));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&curr_data.ss_table_idx), sizeof(curr_data.ss_table_idx));
	crc = crc32(crc, reinterpret_cast<const Bytef *>(&curr_data.entry_cnt), sizeof(curr_data.entry_cnt));

    //start reading entries from ss_table
    uint32_t i = 1;
    while (i <= curr_data.entry_cnt) {
        i++;
        Entry curr_record;
        std::string val;


        // Read operation
        if(!ss_table_file.read(reinterpret_cast<char*>(&curr_record.op), sizeof(curr_record.op))) {
            throw std::runtime_error("corrupted ss_table...");
        }

        // Read key
        if (!ss_table_file.read(reinterpret_cast<char*>(&curr_record.key_len), sizeof(curr_record.key_len))) {
            throw std::runtime_error("corrupted ss_table...");
        }

        curr_record.key.resize(curr_record.key_len);
        if (!ss_table_file.read(curr_record.key.data(), curr_record.key_len)) {
            throw std::runtime_error("corrupted ss_table...");
        }

        // Read value
        if (!ss_table_file.read(reinterpret_cast<char*>(&curr_record.val_len), sizeof(curr_record.val_len))) {
            throw std::runtime_error("corrupted ss_table...");
        }

        val.resize(curr_record.val_len);
        if (!ss_table_file.read(val.data(), curr_record.val_len)) {
            throw std::runtime_error("corrupted ss_table...");
        }

        crc = key_val_checksum(curr_record.op, crc, curr_record.key, val);

        if(curr_record.op == operation::del){
            curr_record.val = std::nullopt;
        }
        else if (curr_record.op == operation::set){
            curr_record.val = val;
        }

        curr_data.records.push_back(curr_record);
    }

    uint32_t checksum;
    ss_table_file.read(reinterpret_cast<char *>(&checksum), sizeof(checksum));

    if(crc != checksum){
        throw std::runtime_error("checksum for ss_table didnt match during read...");
    }

    return curr_data;
}


ss_table::ss_table(uint64_t table_index, open_mode mode){
    std::string ss_table_file_name = SS_TABLE_FILE_NAME + "_" + std::to_string(table_index) + ".bin";
    ss_table_index = table_index;

    if(mode == open_mode::read){
        if(std::filesystem::is_regular_file(ss_table_file_name)){
            ss_table_file.open(ss_table_file_name, std::ios::binary | std::ios::in);
        }
        else{
            throw std::runtime_error("ss_table doesnt exists...");
        }
    }
    else if (mode == open_mode::write){
        { std::ofstream create(ss_table_file_name, std::ios::binary | std::ios::app); }
        ss_table_file.open(ss_table_file_name, std::ios::binary | std::ios::out | std::ios::app);
    }
}


ss_table::~ss_table(){
    ss_table_file.close();
}


bool ss_table::write_to_ss_table(const std::map<std::string, std::optional<std::string>> &mem_table){

    // TODO: need to add return offsets to all blocks

    // HEADER
    write_header_block(this->ss_table_file, this->ss_table_index ,static_cast<uint64_t>(mem_table.size()));

    // DATA BLOCKS
    std::vector<Record> block;
    block.reserve(RECORDS_PER_BLOCK);
    for (const auto& [key, value] : mem_table) {
        block.emplace_back(key, value);

        if (block.size() == RECORDS_PER_BLOCK) {
            write_data_block(ss_table_file, block);
            block.clear();
        }
    }
    if (!block.empty()) {
        write_data_block(ss_table_file, block);
    }

    // BLOOM FILTER


    return ss_table_file.good();
}

// TODO: funtion needs reworking
Lookup_result ss_table::get_value_from_ss_table(const std::string &search_key){

    Lookup_result result = {
        Lookup_status::not_found,
        std::nullopt
    };

    ss_table_data data = this->read_ss_table();
    for(const auto &rec : data.records){
        if (rec.key == search_key) {
            if(rec.op == operation::del){
                result.status = Lookup_status::tombstone;
                result.value = std::nullopt;
            }
            else if (rec.op == operation::set){
                result.status = Lookup_status::found;
                result.value = rec.val;
            }
        }
    }
    
    return result;
}


std::vector<Record> ss_table::get_keys_from_ss_table(const std::optional<std::string> &key){
    ss_table_data data = this->read_ss_table();
    std::map<std::string, std::optional<std::string>> temp_mpp;

    for(const auto &rec : data.records){
        if (rec.op == operation::set) {
            temp_mpp[rec.key] = rec.val;
        }
        else if (rec.op == operation::del) {
            temp_mpp[rec.key] = std::nullopt;
        }
    }

    std::vector<Record> res;
    for(const auto &it : temp_mpp){
        if (key.has_value() && it.first != key.value()) {
            continue;
        }
        res.push_back(it);
    }
    return res;
}


std::vector<Record> ss_table::get_range_from_ss_table(const std::string &start, const std::string &end){

    ss_table_data data = this->read_ss_table();
    std::map<std::string, std::optional<std::string>> temp_mpp;

    for(const auto &rec : data.records){
        if (rec.op == operation::set) {
            temp_mpp[rec.key] = rec.val;
        }
        else if (rec.op == operation::del) {
            temp_mpp[rec.key] = std::nullopt;
        }
    }

    // get range from individual ss_table
    std::vector<Record> res;
    auto it = temp_mpp.lower_bound(start);
    while(it != temp_mpp.end() && it->first <= end){
        res.push_back(*it);
        it++;
    }

    return res;

}


std::vector<Record> ss_table::get_prefix_from_ss_table(const std::string &prefix){

    ss_table_data data = this->read_ss_table();
    std::map<std::string, std::optional<std::string>> temp_mpp;

    for(const auto &rec : data.records){
        if (rec.op == operation::set) {
            temp_mpp[rec.key] = rec.val;
        }
        else if (rec.op == operation::del) {
            temp_mpp[rec.key] = std::nullopt;
        }
    }
    
    // get prefix from individual ss_table
    std::vector<Record> res;
    auto it = temp_mpp.lower_bound(prefix);
    while(it != temp_mpp.end() && (it->first).compare(0, prefix.length(), prefix) == 0){
        res.push_back(*it);
        it++;
    }

    return res;
}

