#pragma once

#include <string>
#include <expected>
#include <span>

#include "WINAPI/TYPES.h"

class PE {

public:

    // Битность образа по Magic из OptionalHeader: 0x010B - PE32 (32 бита), 0x020B - PE32+ (64 бита)
    enum class Format {
        Unknown, // Стандартное значение, битность ещё не определена
        PE32, // x86
        PE32Plus // x64
    };

    // При ошибке парсинга возвращается текст ошибки, объект не создаётся.
    [[nodiscard]]
    static std::expected<PE, std::string> create(std::span<const unsigned char> buffer) {
        PE pe;
        if (auto result = pe.structure_parse(buffer); !result) {
            return std::unexpected(result.error());
        }
        return pe;
    }

private:

    // Только фабрика create() создаёт объект.
    PE() = default;

    // Главная функция парсинга: внутри все проверки (MZ, e_lfanew, PE\0\0, Magic битности)
    // и заполнение заголовков. Читает только, буфер не меняет.
    std::expected<void, std::string> structure_parse(std::span<const unsigned char> buffer);

    _IMAGE_DOS_HEADER         dos_header_ {};
    _IMAGE_FILE_HEADER        file_header_ {};

    IMAGE_OPTIONAL_HEADER32   optional_header32_ {}; // валиден при format_ == Format::PE32
    _IMAGE_OPTIONAL_HEADER64  optional_header64_ {}; // валиден при format_ == Format::PE32Plus

    Format format_ { Format::Unknown };
    QWORD nt_header_offset_ { 0 }; // e_lfanew - оффсет NT-заголовков от начала файла
    QWORD base_address_ { 0 };     // ImageBase из OptionalHeader

public:

    // Битность образа
    [[nodiscard]]
    Format format() const noexcept {
        return format_;
    }

    // 64-битный ли образ (PE32+)
    [[nodiscard]]
    bool b_IsPE64() const noexcept {
        return format_ == Format::PE32Plus;
    }

    // ImageBase из OptionalHeader
    [[nodiscard]]
    QWORD qw_getBaseAddress() const noexcept {
        return base_address_;
    }

    // Оффсет NT-заголовков от начала файла (e_lfanew из DOS-заголовка)
    [[nodiscard]]
    QWORD qw_getNtHeaderAddress() const noexcept {
        return nt_header_offset_;
    }

};
