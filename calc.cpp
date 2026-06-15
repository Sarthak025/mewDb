#include <iostream>
#include <sstream>
#include <string>

void startRepl() {
	std::string expression;
	while (true) {
		std::cout << "mew> ";
		getline(std::cin, expression);

		// exit condition
		if (expression == "exit" || expression == "quit")
			break;

		if (expression.empty())
			continue;

		std::stringstream ss;
		ss.str(expression);

		int a, b;
		char op;
		std::string extra;

		if (!(ss >> a >> op >> b) || (ss >> extra)) {
			std::cout << "ERROR: unknown command" << std::endl;
			continue;
		}

		int res;
		if (op == '+') {
			res = a + b;
		} else if (op == '-') {
			res = a - b;
		} else if (op == '*') {
			res = a * b;
		} else if (op == '/') {
			if (b == 0) {
				std::cout << "ERROR: divisor cannot be 0" << std::endl;
				continue;
			}
			res = a / b;
		} else if (op == '%') {
			if (b == 0) {
				std::cout << "ERROR: modulus cannot be 0" << std::endl;
				continue;
			}
			res = a % b;
		} else {
			std::cout << "ERROR: unknown command" << std::endl;
			continue;
		}

		std::cout << "= " << res << std::endl;
	}
}

int main() {
	startRepl();
	return 0;
}