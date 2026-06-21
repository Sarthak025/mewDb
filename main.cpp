#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string toUpperString(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
				   [](unsigned char c) { return std::toupper(c); });
	return s;
}

void startRepl() {
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

		// check for valid command
		if (command[0] == "SET") {
			std::cout << "Got Command : " << command[0] << std::endl;
		} else if (command[0] == "GET") {
			std::cout << "Got Command : " << command[0] << std::endl;

		} else if (command[0] == "DELETE") {
			std::cout << "Got Command : " << command[0] << std::endl;

		} else if (command[0] == "EXISTS") {
			std::cout << "Got Command : " << command[0] << std::endl;

		} else if (command[0] == "KEYS") {
			std::cout << "Got Command : " << command[0] << std::endl;

		} else if (command[0] == "STATS") {
			std::cout << "Got Command : " << command[0] << std::endl;
		} else {
			std::cout << "ERROR: unknown command" << std::endl;
		}
	}
}

int main() {
	std::cout << "..........Starting mewDb.........." << std::endl;

	startRepl();
}