#include "db_engine.h"
#include "wal.h"
#include "ss_table.h"
#include "constants.h"
#include <iostream>


db_engine::db_engine(const std::string& wal_filename){
    wal_instance = new wal(wal_filename);
    wal_instance->recover(*this);
}

db_engine::~db_engine(){
    delete wal_instance;
}

void db_engine::recover_set(const std::string &key, const std::string &val){
    storage[key] = val;
}

void db_engine::recover_del(const std::string &key){
    storage.erase(key);
}

void db_engine::set(const std::string &key, const std::string &val){
    if(wal_instance->write(SET, key, val)){

        if(this->exists(key)){
            mem_table_size -= key.length() + storage[key].length();
        }

        mem_table_size += key.length() + val.length();
        storage[key] = val;
    }

    if(mem_table_size >= MEM_TABLE_SIZE_LIMIT){
        this->flush();
    }
}

std::optional<std::string> db_engine::get(const std::string &key){
    if(storage.find(key) != storage.end()){
        return storage[key];
    }
    return std::nullopt;
}

bool db_engine::del(const std::string &key){
    if(wal_instance->write(DELETE, key, "")){
        if(this->exists(key)){
            mem_table_size -= key.length() + storage[key].length();
        }
        // 1 for already present key, 0 for not found key
        return storage.erase(key);
    }
    return 0;
}

bool db_engine::exists(const std::string &key){
    return storage.count(key);
}

std::vector<std::string> db_engine::keys(){
    std::vector<std::string> full_data;
    full_data.reserve((storage.size()));

    for(const auto &data : storage){
        full_data.push_back(data.first);
    }
    return full_data;
}

std::vector<std::pair<std::string, std::string>> db_engine::range(const std::string &start, const std::string &end){
    if(start > end){
        return db_engine::range(end, start);
    }

    std::vector<std::pair<std::string, std::string>> data;
    auto it = storage.lower_bound(start);
    
    while(it != storage.end() && it->first <= end){
        data.push_back(*it);
        it++;
    }
    return data;
}

std::vector<std::pair<std::string, std::string>> db_engine::prefix_scan(const std::string &prefix){
    std::vector<std::pair<std::string, std::string>> data;
    auto it = storage.lower_bound(prefix);

    while(it != storage.end() && (it->first).compare(0, prefix.length(), prefix) == 0){
        data.push_back(*it);
        it++;
    }
    return data;
}

void db_engine::flush(){
    ss_table curr_ss_table(SS_TABLE_NAME, this->ss_table_index);
    if(curr_ss_table.write_to_ss_table(storage) && wal_instance->truncate()){
        this->ss_table_index++;
        storage.clear();
        mem_table_size = 0;
    }
}