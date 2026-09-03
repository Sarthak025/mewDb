#include "manifest.h"
#include "constants.h"
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <iostream>


manifest::manifest(const std::string &file_name){
    manifest_file_name = file_name;

    if (std::filesystem::is_regular_file(file_name)) {
        //manifest already exists
        manifest_file.open(file_name, std::ios::in);

        std::string line;

        // get MANIFEST_MAGIC_CONST
        getline(manifest_file, line);
        if(line != MANIFEST_MAGIC_CONST) {
            throw std::runtime_error("Failed to open manifest file...");
        }

        // get VERSION
        getline(manifest_file, line);
        manifest_version = static_cast<uint32_t>(stoi(line));

        // get next_ss_table_index
        getline(manifest_file, line);
        next_ss_table_index = (stoull(line));
        
        // num_ss_tables
        getline(manifest_file, line);
        num_ss_tables = (stoull(line));

    }
    else{
        //create a fresh manifest file

        next_ss_table_index = 0;
        num_ss_tables = 0;

        manifest_file.open(file_name, std::ios::out);
        manifest_file << MANIFEST_MAGIC_CONST << std::endl;
        manifest_file << MANIFEST_VERSION << std::endl;
        manifest_file << next_ss_table_index << std::endl;
        manifest_file << num_ss_tables << std::endl;

        manifest_file.close();
        manifest_file.open(file_name, std::ios::in);
    }
}

manifest::~manifest(){}

std::vector<uint64_t> manifest::get_ss_table_indices(){
    manifest_file.clear();
    manifest_file.seekg(0, std::ios::beg);

    uint32_t line_to_skip = 4;

    std::string line;

    // Skip:
    // 1. MAGIC_NUMBER
    // 2. VERSION
    // 3. NEXT_SS_TABLE_INDEX
    // 4. Read NUM_SS_TABLES
    for (uint32_t i = 0; i < line_to_skip ; i++) {
        if (!std::getline(manifest_file, line)) {
            throw std::runtime_error("Failed to parse manifest file");
        }
    }

    // Read available_indices
    std::vector<uint64_t> available_indices;

    while (std::getline(manifest_file, line)) {
        available_indices.push_back(std::stoull(line));
    }

    return available_indices;
}

uint64_t manifest::get_next_ss_table_index(){
    return next_ss_table_index;
}

bool manifest::add_new_ss_table_index(uint64_t idx){

    std::vector<uint64_t> available_indices = this->get_ss_table_indices();

    next_ss_table_index = idx + 1;
    num_ss_tables += 1;

    //create a temp manifest file
    std::fstream temp_manifest(MANIFEST_TEMP_FILE_NAME, std::ios::out);
    temp_manifest << MANIFEST_MAGIC_CONST << std::endl;
    temp_manifest << MANIFEST_VERSION << std::endl;
    temp_manifest << next_ss_table_index << std::endl;
    temp_manifest << num_ss_tables << std::endl;

    available_indices.push_back(idx);

    for(auto i : available_indices){
        temp_manifest << i << std::endl;
    }
    temp_manifest.flush();

    if (!temp_manifest) {
        return false;
    }

    //rename to old mainfest file
    manifest_file.close();
    std::filesystem::rename(MANIFEST_TEMP_FILE_NAME, manifest_file_name);
    manifest_file.open(manifest_file_name, std::ios::in);
    
    return manifest_file.good();
}