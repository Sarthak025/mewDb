#include "db_engine.h"
#include "wal.h"
#include "ss_table.h"
#include "constants.h"
#include "manifest.h"
#include <iostream>


DbEngine::DbEngine(){
    wal_instance = new Wal();
    wal_instance->recover(*this);
    manifest_instance = new Manifest();
}

DbEngine::~DbEngine(){
    delete wal_instance;
    delete manifest_instance;
}

void DbEngine::recover_set(const std::string &key, const std::string &val){
    if(this->exists_in_curr_mem_table(key)){
        mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
    }

    mem_table_size += key.length() + val.length();
    curr_mem_table[key] = val;
}

void DbEngine::recover_del(const std::string &key){
    if(this->exists_in_curr_mem_table(key)){
        mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
    }
    curr_mem_table[key] = std::nullopt;
}

void DbEngine::set(const std::string &key, const std::string &val){
    if(wal_instance->write(Operation::set, key, val)){

        if(this->exists_in_curr_mem_table(key)){
            mem_table_size -= key.length() + (curr_mem_table[key].value()).length();
        }

        mem_table_size += key.length() + val.length();
        curr_mem_table[key] = val;
    }

    if(mem_table_size >= MEM_TABLE_SIZE_LIMIT){
        if(!(this->flush())){
            throw std::runtime_error("Error in flushing to disk...");
        }
    }
}

std::optional<std::string> DbEngine::get(const std::string &key){

    // Check in curr_mem_table
    if(curr_mem_table.find(key) != curr_mem_table.end()){
        return curr_mem_table[key];
    }

    // Check in SS_Tables
    std::vector<uint64_t> ss_table_indices = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_indices.rbegin(); it != ss_table_indices.rend(); ++it){
        SsTable curr_ss_table(*it, OpenMode::read);
        LookupResult result = curr_ss_table.get_value_from_ss_table(key);

        if(result.status == LookupStatus::not_found) continue;
        else {
            return result.value;
        }
    }

    return std::nullopt;
}

bool DbEngine::del(const std::string &key){
    std::map<std::string, std::optional<std::string>> temp_mem_table = curr_mem_table;

    std::vector<uint64_t> ss_table_idxs = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_idxs.rbegin(); it != ss_table_idxs.rend(); it++){
        auto i = *it;
        SsTable curr_ss_table(i, OpenMode::read);
        std::vector<std::pair<std::string, std::optional<std::string>>> ss_table_records = curr_ss_table.get_keys_from_ss_table(key);
        
        // merge to temp map
        for(const auto &[key, val] : ss_table_records){
            if(temp_mem_table.find(key) == temp_mem_table.end()){
                temp_mem_table[key] = val;
            }
        } 
    }

    
    auto it1 = curr_mem_table.find(key);
    auto it2 = temp_mem_table.find(key);
    
    bool present_in_curr_mem_table = it1 != curr_mem_table.end() && it1->second.has_value();
    bool present_in_temp_mem_table = it2 != temp_mem_table.end() && it2->second.has_value();
    
    if (!present_in_temp_mem_table) {
        return false;
    }

    if (!wal_instance->write(Operation::del, key, "")) {
        throw std::runtime_error("Error in deleting key...");
    }

    if (present_in_curr_mem_table) {
        mem_table_size -= it1->second.value().length();
    }
    else{
        mem_table_size += key.length();
    }

    curr_mem_table[key] = std::nullopt;
    return true;
}

bool DbEngine::exists_in_curr_mem_table(const std::string &key){
    auto it = curr_mem_table.find(key);

    if (it != curr_mem_table.end()) {
        return it->second.has_value();
    }
    return false;
}

std::vector<std::string> DbEngine::keys(){

    // make copy of current mem_table
    std::map<std::string, std::optional<std::string>> temp_mem_table = curr_mem_table;
    std::vector<std::string> full_data;

    std::vector<uint64_t> ss_table_idxs = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_idxs.rbegin(); it != ss_table_idxs.rend(); it++){
        auto i = *it;
        SsTable curr_ss_table(i, OpenMode::read);
        std::vector<std::pair<std::string, std::optional<std::string>>> ss_table_records = curr_ss_table.get_keys_from_ss_table();
        
        // merge to temp map
        for(const auto &[key, val] : ss_table_records){
            if(temp_mem_table.find(key) == temp_mem_table.end()){
                temp_mem_table[key] = val;
            }
        } 
    }

    for(const auto &data : temp_mem_table){
        if(data.second.has_value()){
            full_data.push_back(data.first);
        }
    }
    return full_data;
}

bool DbEngine::exists(const std::string &key){
    return (this->get(key)).has_value();
}

std::vector<std::pair<std::string, std::optional<std::string>>> DbEngine::range(const std::string &start, const std::string &end){

    if(start > end){
        return DbEngine::range(end, start);
    }

    // make copy of current mem_table
    std::map<std::string, std::optional<std::string>> temp_mem_table = curr_mem_table;
    
    std::vector<uint64_t> ss_table_idxs = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_idxs.rbegin(); it != ss_table_idxs.rend(); it++){
        auto i = *it;
        SsTable curr_ss_table(i, OpenMode::read);
        std::vector<std::pair<std::string, std::optional<std::string>>> ss_table_records = curr_ss_table.get_range_from_ss_table(start, end);
        
        // merge to temp map
        for(const auto &[key, val] : ss_table_records){
            if(temp_mem_table.find(key) == temp_mem_table.end()){
                temp_mem_table[key] = val;
            }
        } 
    }
    
    std::vector<std::pair<std::string, std::optional<std::string>>> data;
    auto it = temp_mem_table.lower_bound(start);
    while(it != temp_mem_table.end() && it->first <= end){
        if((*it).second.has_value()){
            data.push_back(*it);
        }
        it++;
    }
    return data;
}

std::vector<std::pair<std::string, std::optional<std::string>>> DbEngine::prefix_scan(const std::string &prefix){
    // make copy of current mem_table
    std::map<std::string, std::optional<std::string>> temp_mem_table = curr_mem_table;
    
    std::vector<uint64_t> ss_table_idxs = manifest_instance->get_ss_table_indices();
    for (auto it = ss_table_idxs.rbegin(); it != ss_table_idxs.rend(); it++){
        auto i = *it;
        SsTable curr_ss_table(i, OpenMode::read);
        std::vector<std::pair<std::string, std::optional<std::string>>> ss_table_records = curr_ss_table.get_prefix_from_ss_table(prefix);
        
        // merge to temp map
        for(const auto &[key, val] : ss_table_records){
            if(temp_mem_table.find(key) == temp_mem_table.end()){
                temp_mem_table[key] = val;
            }
        } 
    }

    std::vector<std::pair<std::string, std::optional<std::string>>> data;
    auto it = temp_mem_table.lower_bound(prefix);
    while(it != temp_mem_table.end() && (it->first).compare(0, prefix.length(), prefix) == 0){
        if((*it).second.has_value()){
            data.push_back(*it);
        }
        it++;
    }
    return data;
}


bool DbEngine::compact(){
    // merge old table
    std::map<std::string, std::optional<std::string>> full_data;
    std::vector<uint64_t> curr_ss_table_indices = manifest_instance->get_ss_table_indices();

    if(curr_ss_table_indices.size() < 2) return true;

    for (auto it = curr_ss_table_indices.rbegin(); it != curr_ss_table_indices.rend(); it++){
        auto i = *it;
        SsTable curr_ss_table(i, OpenMode::read);
        SsTableData curr_data = curr_ss_table.read_ss_table();

        for (const auto &curr_record : curr_data.records){
            if(full_data.find(curr_record.key) == full_data.end()){
                full_data[curr_record.key] = curr_record.val;
            }
        }
    }

    // drop tombstones
    for (auto it = full_data.begin(); it != full_data.end(); ) {
        if (!it->second.has_value()) {
            it = full_data.erase(it);
        } else {
            ++it;
        }
    }

    uint64_t new_index = manifest_instance->get_next_ss_table_index();
    
    // create 1 big new table copy data to big table
    SsTable new_ss_table(new_index,OpenMode::write);
    if(!new_ss_table.write_to_ss_table(full_data)){
        return false;
    }

    // create new temp manifest and rename
    if(!manifest_instance->replace_ss_table_indices(curr_ss_table_indices,new_index)){
        return false;
    }

    // delete old tables
    for(auto idx : curr_ss_table_indices){
        std::string curr_ss_table_name = SS_TABLE_FILE_NAME + "_" + std::to_string(idx) + ".bin";
        if(!std::filesystem::remove(curr_ss_table_name)){
            return false;
        }
    }

    return true;
}


bool DbEngine::flush(){
    uint64_t ss_table_index = manifest_instance->get_next_ss_table_index();
    SsTable curr_ss_table(ss_table_index, OpenMode::write);

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