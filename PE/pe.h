#pragma once

#include "WINAPI/TYPES.h"

class PE64 {

public:
    PE64() = default;
    ~PE64() = default;

private:

    _IMAGE_DOS_HEADER dos_header_;
    _IMAGE_NT_HEADERS64 nt_header_;
    _IMAGE_OPTIONAL_HEADER64 optional_header_;

    QWORD const p_baseAddress {optional_header_.ImageBase};


public:

    QWORD getBaseAddress(){

        return p_baseAddress;
    }

    QWORD getNtHeaderAddress(){

        return p_baseAddress + static_cast<QWORD>(dos_header_.e_lfanew);
    }

};

