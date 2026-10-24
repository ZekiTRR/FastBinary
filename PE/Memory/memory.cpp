#include "memory.h"


std::expected<void, std::string> Memory::loadFile(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (file) {
        buffer = std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return {};
    } else
    {
        return std::unexpected("Failed to open file: " + std::string(path.begin(), path.end()));
    }
};


const std::optional<std::expected<QWORD, std::string> > Memory::pos_pattern_find(const std::vector<UINT> &pattern) const {
    if (pattern.size() > buffer.size()) {

        std::unexpected("Pattern size is larger than buffer size.");
        return std::nullopt;
    }

    const auto it = std::search(buffer.begin(), buffer.end(), pattern.begin(), pattern.end());

    if (it != buffer.end()) {
        return static_cast<QWORD>(it - buffer.begin()); // it указывает на позицию в буффере
    } else {
        return std::nullopt;
    }

}