#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

std::set<std::string> allowed_commands = {"SET",	"GET",	"DELETE",
										  "EXISTS", "KEYS", "STATS"};

void startRepl() {
	while (true) {
		std::cout << "mew>";

		// take input from user
		std::string full_command;
		getline(std::cin, full_command);

		// exit condition
		if (full_command == "exit" || full_command == "quit")
			break;
		if (full_command.empty())
			continue;

        //parse the command
		std::stringstream ss;
		ss.str(full_command);

		std::vector<std::string> command;
		std::string word;

		while (ss >> word) {
			command.push_back(word);
		}

        // check for valid command
		if (allowed_commands.count(command[0])) {
			std::cout << "Got Command : " << command[0] << std::endl;
		} else {
			std::cout << "ERROR: unknown command" << std::endl;
		}

		// for (auto i : command) {
		// 	std::cout << i << std::endl;
		// }
	}
}

int main() {
	std::cout << "..........Starting mewDb.........." << std::endl;

	startRepl();
}