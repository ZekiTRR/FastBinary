#include <iostream>
#include <print>

#include "PE/pe.h"
#include "PE/Memory/memory.h"


int main()
{

	Memory memory;
	PE64 pe;

	if (memory.loadFile(L"parser.exe").has_value()) {
		std::print("File loaded successfully. Buffer size: {} bytes\n", memory.getBuffer().size());



		auto test = pe.b_IsPE64(memory);
		if (test) {
			std::print("The file is a PE64 executable.\n");
		} else {
			std::print("The file is NOT a PE64 executable.\n");
		}


	} else {
		std::cerr << memory.loadFile(L"parser.exe").error() << std::endl;
		return 1;
	}



	std::cin >> std::ws;
	return 0;
}
