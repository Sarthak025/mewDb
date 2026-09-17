#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#define XXH_INLINE_ALL
#include <xxhash.h>
#include <algorithm>
#include <cctype>

std::string toUpperString(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
				   [](unsigned char c) { return std::toupper(c); });
	return s;
}

bool checkForArguments(const int numOfAvailableArguments,
					   const int numOfargumentsNeeded) {
	if (numOfargumentsNeeded != numOfAvailableArguments) {
		std::cout << "ERROR: Not exact arguments, Required "
				  << numOfargumentsNeeded << " Arguments" << std::endl;
		return false;
	}

	return true;
}

std::string trim(std::string s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
				return !std::isspace(ch);
			}));

	s.erase(std::find_if(s.rbegin(), s.rend(),
						 [](unsigned char ch) { return !std::isspace(ch); })
				.base(),
			s.end());

	return s;
}

std::vector<uint8_t> bloomFilter(180, 0);

uint64_t xxhash(const std::string &key, uint64_t seed) {

	return XXH3_64bits_withSeed(key.data(), key.size(), seed);
}

void insertViaBloomFilter(const std::string &key) {
	uint64_t h1 = xxhash(key, 123);
	uint64_t h2 = xxhash(key, 456);

	for (uint64_t i = 0; i < 10; i++) {
		uint64_t hash = h1 + i * h2;

		uint64_t bit_idx = hash % 1440;
		uint64_t byte_idx = bit_idx / 8;
		uint64_t bit_offset = bit_idx % 8;

		bloomFilter[byte_idx] |= (1u << bit_offset);
	}
}

std::string getFromBloomFilter(const std::string &key) {
	uint64_t h1 = xxhash(key, 123);
	uint64_t h2 = xxhash(key, 456);

	for (uint64_t i = 0; i < 10; i++) {
		uint64_t hash = h1 + i * h2;

		uint64_t bit_idx = hash % 1440;
        uint64_t byte_idx = bit_idx / 8;
		uint64_t bit_offset = bit_idx % 8;

		uint8_t temp = bloomFilter[byte_idx];
		if ((temp & (1u << bit_offset)) == 0) {
			return "NOT PRESENT";
		}
	}
	return "PRESENT";
}

int main() {
	while (true) {
		// take input from user
		std::string full_command;
		getline(std::cin, full_command);
		if (!std::cin) {
			break;
		}

		// exit condition
		if (trim(toUpperString(full_command)) == "EXIT" ||
			trim(toUpperString(full_command)) == "QUIT")
			break;
		if (full_command.empty())
			continue;

		// parse the command
		std::stringstream ss;
		ss.str(full_command);

		std::vector<std::string> command;
		std::string word;

		// Capitalise the first word (COMMAND TYPE)
		ss >> word;
		command.push_back(toUpperString(word));
		while (ss >> word) {
			command.push_back(word);
		}

		int numOfArguments = (int)command.size() - 1;

		// check for valid command
		if (command[0] == "INSERT") {
			if (!checkForArguments(numOfArguments, 1))
				continue;

			insertViaBloomFilter(command[1]);

			std::cout << "OK" << std::endl;

		} else if (command[0] == "GET") {
			if (!checkForArguments(numOfArguments, 1))
				continue;

			std::cout << getFromBloomFilter(command[1]);
            std::cout << std::endl;

		} else {
			std::cout << "ERROR: unknown command" << std::endl;
		}
	}

	return 0;
}