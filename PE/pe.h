#pragma once

#include <string>
#include <optional>
#include <expected>

#include "Memory/memory.h"
#include "WINAPI/TYPES.h"

class PE {

public:

    // Битность образа по Magic из OptionalHeader: 0x010B - PE32 (32 бита), 0x020B - PE32+ (64 бита)
    enum class Format {
        Unknown, // Не PE или не удалось спарсить
        PE32, // x86
        PE32Plus // x64
    };

    // Конструктор сразу парсит структуру PE из загруженного в Memory буфера.
    // Все проверки - внутри structure_parse.
    explicit PE(const Memory& memory) {
        if (auto result = structure_parse(memory); !result) {
            parse_error_ = std::move(result.error());
        }
    }


private:

    // Главная функция парсинга: внутри все проверки (MZ, e_lfanew, PE\0\0, Magic битности)
    // и заполнение заголовков. Читает только, ничего в объекте memory не меняет.
    std::expected<void, std::string> structure_parse(const Memory& memory);

    _IMAGE_DOS_HEADER         dos_header_ {};
    _IMAGE_FILE_HEADER        file_header_ {};
    IMAGE_OPTIONAL_HEADER32   optional_header32_ {}; // валиден при format_ == Format::PE32
    _IMAGE_OPTIONAL_HEADER64  optional_header64_ {}; // валиден при format_ == Format::PE32Plus

    Format format_ { Format::Unknown };
    QWORD nt_header_offset_ { 0 }; // e_lfanew - оффсет NT-заголовков от начала файла
    QWORD base_address_ { 0 };     // ImageBase из OptionalHeader
    std::optional<std::string> parse_error_;


public:

    // Спарсилась ли структура (конструктор парсит сразу - здесь результат этого парсинга)
    [[nodiscard]]
    bool isParsed() const noexcept {
        return !parse_error_.has_value();
    }

    // Причина неудачного парсинга (пустая, если isParsed() == true)
    [[nodiscard]]
    const std::optional<std::string>& parseError() const noexcept {
        return parse_error_;
    }

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
