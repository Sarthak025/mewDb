#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "db_engine.h"

std::string toUpperString(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
				   [](unsigned char c) { return std::toupper(c); });
	return s;
}

bool checkForArguments(const int numOfAvailableArguments, const int numOfargumentsNeeded){
	if(numOfargumentsNeeded != numOfAvailableArguments){
		std::cout << "ERROR: Not enough arguments, Required " << numOfargumentsNeeded << " Arguments" << std::endl;
		return false;
	}

	return true;
}

void startRepl(db_engine &Db) {
	while (true) {
		std::cout << "mew> ";

		// take input from user
		std::string full_command;
		getline(std::cin, full_command);

		// exit condition
		if (toUpperString(full_command) == "EXIT" ||
			toUpperString(full_command) == "QUIT")
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
		if (command[0] == "SET") {
			if(!checkForArguments(numOfArguments, 2)) continue;

			Db.set(command[1], command[2]);
			std::cout << "OK" << std::endl;

		} else if (command[0] == "GET") {
			if(!checkForArguments(numOfArguments, 1)) continue;

			std::optional<std::string> val = Db.get(command[1]);
			if(val){
				std::cout << "VALUE " << val.value() << std::endl;
				
			}else{
				std::cout << "NOT_FOUND" << std::endl;
			}

		} else if (command[0] == "DELETE") {
			if(!checkForArguments(numOfArguments, 1)) continue;

			int deleteStatus = Db.del(command[1]);
			if(deleteStatus){
				std::cout << "OK" << std::endl;
			}else{
				std::cout << "NOT_FOUND" << std::endl;
				
			}
			
		} else if (command[0] == "EXISTS") {
			if(!checkForArguments(numOfArguments, 1)) continue;

			if(Db.exists(command[1])){
				std::cout << "TRUE" << std::endl;
			}else{
				std::cout << "FALSE" << std::endl;
			}

		} else if (command[0] == "KEYS") {
			if(!checkForArguments(numOfArguments, 0)) continue;

			std::vector<std::string> fullData =  Db.keys();
			for(const auto &data : fullData){
				std::cout << data << std::endl;
			}
			std::cout << "END" << std::endl;

		} else {
			std::cout << "ERROR: unknown command" << std::endl;
		}
	}
}

int main() {
	std::cout << "..........Starting mewDb.........." << std::endl;
	db_engine Db;
	startRepl(Db);
}