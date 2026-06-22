#include "db_engine.h"

db_engine::db_engine(){}

db_engine::~db_engine(){}

void db_engine::set(const std::string &key, const std::string &val){
    storage[key] = val;
}

std::optional<std::string> db_engine::get(const std::string &key){
    if(storage.find(key) != storage.end()){
        return storage[key];
    }
    return std::nullopt;
}

int db_engine::del(const std::string &key){
    // 1 for already present key, 0 for not found key
    return storage.erase(key);
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