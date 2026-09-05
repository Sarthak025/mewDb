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
    if(this->exists_in_curr_mem_table(key)){
        mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
    }

    mem_table_size += key.length() + val.length();
    curr_mem_table[key] = val;
}

void db_engine::recover_del(const std::string &key){
    if(this->exists_in_curr_mem_table(key)){
        mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
    }
    curr_mem_table[key] = std::nullopt;
}

void db_engine::set(const std::string &key, const std::string &val){
    if(wal_instance->write(operation::set, key, val)){

        if(this->exists_in_curr_mem_table(key)){
            mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
        }

        mem_table_size += key.length() + val.length();
        curr_mem_table[key] = val;
    }

    if(mem_table_size >= MEM_TABLE_SIZE_LIMIT){
        if(!(this->flush())){
            // TODO: add log in future for flush failing
        }
    }
}

std::optional<std::string> db_engine::get(const std::string &key){

    // Check in curr_mem_table
    if(curr_mem_table.find(key) != curr_mem_table.end()){
        return curr_mem_table[key];
    }

    // Check in SS_Tables
    std::vector<uint64_t> ss_table_indices = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_indices.rbegin(); it != ss_table_indices.rend(); ++it){
        ss_table curr_ss_table(*it, open_mode::read);
        lookup_result result = curr_ss_table.get_value_from_ss_table(key);

        if(result.status == lookup_status::not_found) continue;
        else {
            return result.value;
        }
    }

    return std::nullopt;
}

bool db_engine::del(const std::string &key){
    // TODO: to return false search in all ss_tables too
    
    if(wal_instance->write(operation::del, key, "")){
        if(this->exists_in_curr_mem_table(key)){
            mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
        }
        curr_mem_table[key] = std::nullopt;
        return true;
    }
    else{
        throw std::runtime_error("Error in deleting key...");
    }
}

bool db_engine::exists_in_curr_mem_table(const std::string &key){
    auto it = curr_mem_table.find(key);

    if (it != curr_mem_table.end()) {
        return it->second.has_value();
    }

    // TODO: need to add ss_table search later
    return false;
}

std::vector<std::string> db_engine::keys(){
    std::vector<std::string> full_data;
    full_data.reserve((curr_mem_table.size()));

    for(const auto &data : curr_mem_table){
        if(data.second.has_value()){
            full_data.push_back(data.first);
        }
    }
    return full_data;
}

std::vector<std::pair<std::string, std::optional<std::string>>> db_engine::range(const std::string &start, const std::string &end){
    // TODO: add a search from ss_Table too

    if(start > end){
        return db_engine::range(end, start);
    }

    std::vector<std::pair<std::string, std::optional<std::string>>> data;
    auto it = curr_mem_table.lower_bound(start);
    
    while(it != curr_mem_table.end() && it->first <= end){
        if((*it).second.has_value()){
            data.push_back(*it);
        }
        it++;
    }
    return data;
}

std::vector<std::pair<std::string, std::optional<std::string>>> db_engine::prefix_scan(const std::string &prefix){
    // TODO: add a search from ss_Table too

    std::vector<std::pair<std::string, std::optional<std::string>>> data;
    auto it = curr_mem_table.lower_bound(prefix);

    while(it != curr_mem_table.end() && (it->first).compare(0, prefix.length(), prefix) == 0){
        if((*it).second.has_value()){
            data.push_back(*it);
        }
        it++;
    }
    return data;
}

bool db_engine::flush(){
    uint64_t ss_table_index = manifest_instance->get_next_ss_table_index();
    ss_table curr_ss_table(ss_table_index, open_mode::write);

    if (!curr_ss_table.write_to_ss_table(curr_mem_table)) {
        return false;
    }

    if (!manifest_instance->add_new_ss_table_index(ss_table_index)) {
        return false;
    }

    if (!wal_instance->truncate()) {
        return false;
    }

    curr_mem_table.clear();
    mem_table_size = 0;

    return true;
}