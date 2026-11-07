#include <iostream>
#include <print>

#include "PE/pe.h"
#include "PE/Memory/memory.h"


int main()
{

	// В Qt объект создаётся при бросании файла: путь достаётся из дропа и сразу идёт в фабрику.
	// Здесь для примера путь задаётся вручную.
	auto memory_result = Memory::load(L"parser.exe");

	if (!memory_result) {
		std::cerr << memory_result.error() << std::endl;
		return 1;
	}
	const Memory& memory = *memory_result;

	std::print("File's SHA256: {}\n", memory.getHash256());
	std::print("File's MD5: {}\n", memory.getHashMD5());

	// Фабрика PE сразу делает парс структур
	auto pe_result = PE::create(memory.getBuffer());

	if (!pe_result) {
		std::cerr << pe_result.error() << std::endl;
		return 1;
	}
	const PE& pe = *pe_result;

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
