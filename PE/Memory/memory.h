#pragma once

#include <vector>
#include <string>
#include <optional>
#include <expected>
#include <span>

#include "PE/hash.h"
#include "WINAPI/TYPES.h"

class Memory {

private:

    // Только фабрика load() создаёт объект и только с уже загруженным буфером.
    explicit Memory(std::vector<unsigned char> buffer)
        : buffer_(std::move(buffer))
        , Hash256_(calcHash256(buffer_))
        , HashMD5_(calcHashMD5(buffer_))
    {
    }

    std::vector<unsigned char> buffer_;
    std::string Hash256_;
    std::string HashMD5_;

public:

    // Фабрика: объект существует только если файл успешно загружен.
    // При ошибке возвращается текст ошибки, объект не создаётся.
    [[nodiscard]]
    static std::expected<Memory, std::string> load(const std::wstring& path);

    // Возвращает span на буфер
    [[nodiscard]]
    std::span<const unsigned char> getBuffer() const noexcept {
        return std::span<const unsigned char>(buffer_);
    }

    // Поиск паттерна в буффере: возвращает оффсет (позицию) от начала,
    [[nodiscard]]
    std::optional<QWORD> pos_pattern_find(const std::vector<UINT>& pattern) const;

    [[nodiscard]]
    const std::string& getHash256() const noexcept {
        return Hash256_;
    }

    [[nodiscard]]
    const std::string& getHashMD5() const noexcept {
        return HashMD5_;
    }
};
