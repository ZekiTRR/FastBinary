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

	std::print("File's SHA256: {}\n", memory.getHash256());
	std::print("File's MD5: {}\n", memory.getHashMD5());

	// Конструктор PE сразу делает парс структур
	PE pe(memory);

	if (!pe.isParsed()) {
		std::cerr << pe.parseError().value_or("Unknown error") << std::endl;
		return 1;
	}

	if (pe.b_IsPE64()) {
		std::print("The file is a PE64 (PE32+) executable.\n");
		std::print("ImageBase: 0x{:X}\n", pe.qw_getBaseAddress());
	} else {
		std::print("The file is a PE32 executable.\n");
		std::print("ImageBase: 0x{:X}\n", pe.qw_getBaseAddress());
	}

	std::cin >> std::ws;
	return 0;
}
