#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

std::string toUpperString(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
				   [](unsigned char c) { return std::toupper(c); });
	return s;
}

void write_to_file() {
	std::vector<std::string> input;

	std::string s;
	while (toUpperString(s) != "END") {
		std::cin >> s;
		input.push_back(s);
	}
	input.pop_back();

	std::ofstream file("temp_data.bin", std::ios::binary); // ios::app
	if (!file) {
		std::cout << "File could not be opened\n";
		return;
	}

	for (std::string &st : input) {
		uint32_t len = st.length();
		file.write(reinterpret_cast<char *>(&len), sizeof(len));
		file.write(st.c_str(), len);
	}

	file.close();

	return;
}

void read_from_file() {

	std::ifstream file("temp_data.bin", std::ios::binary);

	while (true) {
		uint32_t len;
		file.read(reinterpret_cast<char *>(&len), sizeof(len));
		if (!file) {
			break;;
		}

		std::string s(len, '\0');
		file.read(&s[0], len);

		if (!file) {
			break;
		}

		std::cout << s << '\n';
	}

	file.close();
}

int main() {

	std::string task;
	while (toUpperString(task) != "EXIT") {

		std::cout << "CHOOSE TASK TO DO......\n";
		std::cin >> task;

		if (toUpperString(task) == "WRITE") {
			std::cout << "WRITING......\n";
			write_to_file();
		} else if (toUpperString(task) == "READ") {
			std::cout << "READING......\n";
			read_from_file();
		}
	}
}