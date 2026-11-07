#include "memory.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>


// Загрузка файла в буфер (вектор)
std::expected<Memory, std::string> Memory::load(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::unexpected("Failed to open file: " + std::filesystem::path(path).string());
    }

    std::vector<unsigned char> buffer;
    buffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

    return Memory(std::move(buffer));
}


// Поиск паттерна в буффере. При успешном нахождении возвращает оффсет (позицию) в буффере от начала
std::optional<QWORD> Memory::pos_pattern_find(const std::vector<UINT>& pattern) const {
    const auto it = std::search(buffer_.begin(), buffer_.end(), pattern.begin(), pattern.end());

    if (it == buffer_.end()) {
        return std::nullopt;
    }

    return static_cast<QWORD>(it - buffer_.begin());
}
