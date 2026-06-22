#pragma once

#include <string>
#include <map>
#include <optional>
#include <vector>

class db_engine {
  private:
    std::map<std::string, std::string> storage;

  public:
    db_engine();
    ~db_engine();
    void set(const std::string &key, const std::string &val);
    std::optional<std::string> get(const std::string& key);
    int del(const std::string &key);
    bool exists(const std::string &key);
    std::vector<std::string> keys();
    std::vector<std::pair<std::string, std::string>> range(const std::string &start, const std::string &end);
    std::vector<std::pair<std::string, std::string>> prefix_scan(const std::string &prefix);

};
