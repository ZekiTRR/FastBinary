#include <iostream>
#include <print>

#include "PE/pe.h"
#include "PE/Memory/memory.h"


int main()
{

	// В Qt объект создаётся при бросании файла: путь достаётся из дропа и сразу идёт в конструктор.
	// Здесь для примера путь задаётся вручную.
	Memory memory(L"parser.exe");

	if (!memory.isLoaded()) {
		std::cerr << memory.loadError().value_or("Unknown error") << std::endl;
		return 1;
	}

	std::print("File loaded successfully. Buffer size: {} bytes\n", memory.getBuffer().size());

	// Конструктор PE сразу делает парс структуры
	PE pe(memory);

	if (!pe.isParsed()) {
		std::cerr << pe.parseError().value_or("Unknown error") << std::endl;
		return 1;
	}

	if (pe.b_IsPE64()) {
		std::print("The file is a PE64 (PE32+) executable.\n");
	} else {
		std::print("The file is a PE32 executable.\n");
	}

	std::print("NT headers offset: {}, image base: 0x{:X}\n", pe.qw_getNtHeaderAddress(), pe.qw_getBaseAddress());



	std::cin >> std::ws;
	return 0;
}
