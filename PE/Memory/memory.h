#pragma once

#include <vector>
#include <fstream>
#include <expected>
#include <optional>
#include <algorithm>

#include "WINAPI/TYPES.h"

class Memory {

public:

    Memory() = default;
    ~Memory() = default;


private:

    std::vector<unsigned char> buffer;



public:


    std::expected<void, std::string> loadFile(const std::wstring& path);

    [[nodiscard]]
    const std::vector<unsigned char>& getBuffer() const {
        return buffer;
    }

    // Возвращает кол-во байт от начала - оффсет
    [[nodiscard]]
   const std::optional<std::expected<QWORD, std::string>> pos_pattern_find(const std::vector<UINT>& pattern) const;

};
