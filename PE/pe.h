#pragma once

#include <vector>

#include "Memory/memory.h"
#include "WINAPI/TYPES.h"

class PE64 {

public:
    PE64() = default;
    ~PE64() = default;

private:

    _IMAGE_DOS_HEADER dos_header_;
    _IMAGE_NT_HEADERS64 nt_header_; // DOS_HEADER.e_lfanew -> NT_HEADER
    _IMAGE_OPTIONAL_HEADER64 optional_header_;

    QWORD const p_baseAddress {optional_header_.ImageBase};


public:

    [[nodiscard]]
    QWORD getBaseAddress() const{

        return p_baseAddress;
    }


    [[nodiscard]]
    QWORD getNtHeaderAddress() const{

        return p_baseAddress + static_cast<QWORD>(dos_header_.e_lfanew);
    }


// От PE + 2 байта пустых и следующий байт  если там 64 -> PE64
bool b_IsPE64(Memory& memory) const;
	
};

