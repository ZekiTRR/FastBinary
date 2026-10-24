#include "pe.h"
#include "Memory/memory.h"


bool PE64::b_IsPE64(Memory& memory) const {

	auto found = memory.pos_pattern_find({ 0x50, 0x45, 0x00 , 0x00, 0x64, 0x86 }); // found: nullopt

	if (found.has_value()) {
		if (found.value().has_value()) {
			return true;
		}
	} else
	{
		return false;
	}


}