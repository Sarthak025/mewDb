#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

class db_engine {
  private:
    std::unordered_map<std::string, std::string> storage;

  public:
    db_engine();
    ~db_engine();
    void set(const std::string &key, const std::string &val);
    std::optional<std::string> get(const std::string& key);
    int del(const std::string &key);
    bool exists(const std::string &key);
    std::vector<std::string> keys();

};
