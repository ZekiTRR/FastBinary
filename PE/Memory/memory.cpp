#include "memory.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>


// Загрузка файла в буффер (вектор)
std::expected<void, std::string> Memory::loadFile(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        load_error_ = "Failed to open file: " + std::filesystem::path(path).string();
        return std::unexpected(*load_error_);
    }



    buffer_.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    load_error_.reset();
    return {};
}


// Поиск паттерна в буффере. При успешном нахождении возвращает оффсет (позицию) в буффере от начала
std::optional<QWORD> Memory::pos_pattern_find(const std::vector<UINT>& pattern) const {
    const auto it = std::search(buffer_.begin(), buffer_.end(), pattern.begin(), pattern.end());

    if (it == buffer_.end()) {
        return std::nullopt;
    }

    return static_cast<QWORD>(it - buffer_.begin());
}
