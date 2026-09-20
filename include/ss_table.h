#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <map>
#include <optional>
#include "constants.h"

// Format of SS_TABLE
// [ Header ]                 <- fixed size, offset 0
// [ Data Block 0 ]
// [ Data Block 1 ]
// ...
// [ Data Block N-1 ]
// [ Bloom Filter Block ]      <- starts at footer.bloom_filter_offset
// [ Sparse Index Block ]      <- starts at footer.sparse_index_offset
// [ Footer ]                  <- fixed size, last thing in the file
//________________________________________________________________________________________________________________________________

// HEADER BLOCK (FIXED BYTE SIZE)
// uint32_t magic_number;
// uint8_t version_number;
// uint64_t ss_table_index;
// uint64_t entry_count;
// uint32_t header_checksum;
//________________________________________________________________________________________________________________________________

// DATA BLOCK
// uint32_t entry_cnt;
// 
// (multiple records)
// uint8_t operation; (from the shared enum: set / del)
// uint32_t key_len;
// char[] key;
// uint32_t val_len; (0 for a del entry)
// char[] val; (empty for a del entry)
// 
// uint32_t data_block_checksum;
//________________________________________________________________________________________________________________________________

// BLOOM FILTER BLOCK
// uint64_t bit_array_size;
// uint32_t hash_func_cnt;
// uint8_t[] bit_array;
// uint32 bloom_filter_checksum;
//________________________________________________________________________________________________________________________________

// SPARSE INDEX BLOCK
// uint64_t num_data_blocks;
// 
// (muliple sparse indexes)
// uint32_t key_len;
// char[] key; (variable)
// uint64_t offset;
// uint32_t sparse_index_checksum;
//________________________________________________________________________________________________________________________________

// FOOTER BLOCK (FIXED BYTE SIZE)
// uint64_t bloom_filter_offset;
// uint64_t sparse_index_offset;
// uint32_t footer_checksum;



enum class LookupStatus { 
	not_found, 
	tombstone,
	found
};

struct LookupResult {
    LookupStatus status;
    std::optional<std::string> value;  // meaningful only when status == found
};

struct Entry {
    Operation op;
    uint32_t key_len;
    std::string key;
    uint32_t val_len;
    std::optional<std::string> val;
};

struct SsTableData {
    uint32_t magic_num;
    uint8_t version_num;
    uint64_t ss_table_idx;
    uint64_t entry_cnt;
    std::vector<Entry> records;
};


class SsTable {
private:
	uint64_t ss_table_index;
	std::fstream ss_table_file;
    
    public:
	SsTable(uint64_t table_index, OpenMode mode);
	~SsTable();
    
    bool write_to_ss_table(const std::map<std::string, std::optional<std::string>> &mem_table);
	LookupResult get_value_from_ss_table(const std::string &key);
	std::vector<std::pair<std::string, std::optional<std::string>>> get_range_from_ss_table(const std::string &start, const std::string &end);
	std::vector<std::pair<std::string, std::optional<std::string>>> get_prefix_from_ss_table(const std::string &prefix);
    std::vector<std::pair<std::string, std::optional<std::string>>> get_keys_from_ss_table(const std::optional<std::string> &key = std::nullopt);
	SsTableData read_ss_table();

};