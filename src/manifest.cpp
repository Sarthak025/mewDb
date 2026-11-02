#include "manifest.h"
#include "constants.h"
#include "file_io.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <system_error>

// Returns the values of curr that are not in remove. Both vectors must be sorted in ascending order.
std::vector<uint64_t> subtract_vectors(const std::vector<uint64_t> &curr, const std::vector<uint64_t> &remove) {
    std::vector<uint64_t> result;

    uint64_t i = 0;
    uint64_t j = 0;

    while(i < curr.size() && j < remove.size()){

        if(curr[i] == remove[j]){
            i++;
            j++;
        }
        else if(curr[i] < remove[j]){
            result.push_back(curr[i]);
            i++;
        }
        else{
            j++;
        }
    }

    while(i < curr.size()){
        result.push_back(curr[i]);
        i++;
    }

    return result;
}

// If the manifest file exists, opens it for reading and loads its magic, version, next index and SSTable
// count (throws on a wrong magic). Otherwise creates an empty manifest with next index 0.
Manifest::Manifest(){
    manifest_file_name = MANIFEST_FILE_NAME;

    if (std::filesystem::is_regular_file(manifest_file_name)) {
        //manifest already exists
        manifest_file.open(manifest_file_name, std::ios::in);

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
        if (!write_manifest(0, {})) {
            throw std::runtime_error("Failed to create manifest file...");
        }
    }
}


// Manifest_file closes itself.
Manifest::~Manifest(){}


// Re-reads the manifest file and returns its SSTable indices in file order (oldest first).
// Throws if the 4 header lines are missing.
std::vector<uint64_t> Manifest::get_ss_table_indices(){
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


// Returns the index the next new SSTable should use (cached in memory, not re-read from the file).
uint64_t Manifest::get_next_ss_table_index(){
    return next_ss_table_index;
}


// Appends idx to the SSTable list and durably rewrites the manifest with next index idx + 1.
bool Manifest::add_new_ss_table_index(uint64_t idx){

    std::vector<uint64_t> available_indices = this->get_ss_table_indices();
    available_indices.push_back(idx);

    return write_manifest(idx + 1, available_indices);
}


// Removes old_indices from the SSTable list, appends new_index as the newest entry, and durably
// rewrites the manifest with next index new_index + 1. Used by compaction.
bool Manifest::replace_ss_table_indices(const std::vector<uint64_t> &old_indices, uint64_t new_index){
    std::vector<uint64_t> curr_indices = this->get_ss_table_indices();
    std::vector<uint64_t> final_indices = subtract_vectors(curr_indices, old_indices);

    final_indices.push_back(new_index);

    return write_manifest(new_index + 1, final_indices);
}


// Durably replaces the manifest: writes the full contents to the temp file and syncs it, renames it over
// the manifest, reopens the manifest for reading, updates the in-memory counters, then syncs the directory.
// Returns false at the first step that fails; the counters change only once the rename has succeeded.
bool Manifest::write_manifest(uint64_t next_index, const std::vector<uint64_t> &indices){
    std::string contents = MANIFEST_MAGIC_CONST + "\n"
        + std::to_string(MANIFEST_VERSION) + "\n"
        + std::to_string(next_index) + "\n"
        + std::to_string(indices.size()) + "\n";
    for(auto i : indices){
        contents += std::to_string(i) + "\n";
    }

    // Syncing temp file before rename else rename can reach disk before contents
    std::FILE *temp_manifest = std::fopen(MANIFEST_TEMP_FILE_NAME.c_str(), "wb");
    if (!temp_manifest) {
        return false;
    }
    bool written = write_bytes(temp_manifest, contents.data(), contents.size()) && sync_file(temp_manifest);
    if (std::fclose(temp_manifest) != 0 || !written) {
        return false;
    }

    //rename temp manifest file to original
    manifest_file.close();
    std::error_code rename_error;
    std::filesystem::rename(MANIFEST_TEMP_FILE_NAME, manifest_file_name, rename_error);
    manifest_file.open(manifest_file_name, std::ios::in);
    if (rename_error) {
        return false;
    }

    next_ss_table_index = next_index;
    num_ss_tables = indices.size();

    return sync_parent_directory(manifest_file_name) && manifest_file.good();
}