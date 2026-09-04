#include "db_engine.h"
#include "wal.h"
#include "ss_table.h"
#include "constants.h"
#include "manifest.h"
#include <iostream>


db_engine::db_engine(){
    wal_instance = new wal(WAL_FILE_NAME);
    wal_instance->recover(*this);
    manifest_instance = new manifest(MANIFEST_FILE_NAME);
}

db_engine::~db_engine(){
    delete wal_instance;
    delete manifest_instance;
}

void db_engine::recover_set(const std::string &key, const std::string &val){
    curr_memTable[key] = val;
}

void db_engine::recover_del(const std::string &key){
    curr_memTable.erase(key);
}

void db_engine::set(const std::string &key, const std::string &val){
    if(wal_instance->write(SET, key, val)){

        if(this->exists(key)){
            mem_table_size -= key.length() + curr_memTable[key].length();
        }

        mem_table_size += key.length() + val.length();
        curr_memTable[key] = val;
    }

    if(mem_table_size >= MEM_TABLE_SIZE_LIMIT){
        if(!(this->flush())){
            // TODO: add log in future for flush failing
        }
    }
}

std::optional<std::string> db_engine::get(const std::string &key){

    // Check in curr_memTable
    if(curr_memTable.find(key) != curr_memTable.end()){
        return curr_memTable[key];
    }

    // Check in SS_Tables
    std::vector<uint64_t> ss_table_indices = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_indices.rbegin(); it != ss_table_indices.rend(); ++it){
        ss_table curr_ss_table(*it, open_mode::read);
        std::optional<std::string> val = curr_ss_table.get_value_from_ss_table(key);
        if(val){
            return val.value();
        }
    }
    return std::nullopt;
}

bool db_engine::del(const std::string &key){
    if(wal_instance->write(DELETE, key, "")){
        if(this->exists(key)){
            mem_table_size -= key.length() + curr_memTable[key].length();
        }
        // 1 for already present key, 0 for not found key
        return curr_memTable.erase(key);
    }
    return 0;
}

bool db_engine::exists(const std::string &key){
    return curr_memTable.count(key);
}

std::vector<std::string> db_engine::keys(){
    std::vector<std::string> full_data;
    full_data.reserve((curr_memTable.size()));

    for(const auto &data : curr_memTable){
        full_data.push_back(data.first);
    }
    return full_data;
}

std::vector<std::pair<std::string, std::string>> db_engine::range(const std::string &start, const std::string &end){
    if(start > end){
        return db_engine::range(end, start);
    }

    std::vector<std::pair<std::string, std::string>> data;
    auto it = curr_memTable.lower_bound(start);
    
    while(it != curr_memTable.end() && it->first <= end){
        data.push_back(*it);
        it++;
    }
    return data;
}

std::vector<std::pair<std::string, std::string>> db_engine::prefix_scan(const std::string &prefix){
    std::vector<std::pair<std::string, std::string>> data;
    auto it = curr_memTable.lower_bound(prefix);

    while(it != curr_memTable.end() && (it->first).compare(0, prefix.length(), prefix) == 0){
        data.push_back(*it);
        it++;
    }
    return data;
}

bool db_engine::flush(){
    uint64_t ss_table_index = manifest_instance->get_next_ss_table_index();
    ss_table curr_ss_table(ss_table_index, open_mode::write);

    if (!curr_ss_table.write_to_ss_table(curr_memTable)) {
        return false;
    }

    if (!manifest_instance->add_new_ss_table_index(ss_table_index)) {
        return false;
    }

    if (!wal_instance->truncate()) {
        return false;
    }

    curr_memTable.clear();
    mem_table_size = 0;

    return true;
}