#pragma once

#include <expected>
#include <span>
#include <string>
#include <variant>

#include "WINAPI/TYPES.h"

// Разбор отдельных структур PE-файла из сырого буфера.
// Каждая подфункция читает одну структуру по оффсету и проверяет её валидность,
// поэтому наружу выходит только корректная структура (или текст ошибки).
// Главный парс PE::structure_parse вызывает их по очереди и заполняет поля объекта.
// Подфункции - свободные функции, а не методы PE: им не нужен доступ к состоянию
// объекта, только к буферу (CppCoreGuidelines C.4).

// Читает DOS-заголовок с начала буфера: файл должен вмещать структуру,
// подпись должна быть 'MZ', e_lfanew не должен указывать внутрь DOS-заголовка.
[[nodiscard]]
std::expected<_IMAGE_DOS_HEADER, std::string> dos_header_parse(std::span<const unsigned char> buffer);

// Проверяет подпись NT-заголовков 'PE\0\0' по оффсету от начала файла.
// Ничего не возвращает: подпись - проверка, хранить её значение не нужно.
[[nodiscard]]
std::expected<void, std::string> nt_signature_check(std::span<const unsigned char> buffer, QWORD nt_header_offset);

// Читает FileHeader, который находится сразу за подписью NT-заголовков.
[[nodiscard]]
std::expected<_IMAGE_FILE_HEADER, std::string> file_header_parse(std::span<const unsigned char> buffer, QWORD nt_header_offset);

// OptionalHeader бывает двух видов, поэтому возвращается variant:
// активная альтернатива и есть формат образа (index 0 - PE32, index 1 - PE32+).
using OptionalHeader = std::variant<IMAGE_OPTIONAL_HEADER32, _IMAGE_OPTIONAL_HEADER64>;

// Читает OptionalHeader по оффсету, битность определяется по Magic:
// 0x010B - PE32 (32 бита), 0x020B - PE32+ (64 бита).
[[nodiscard]]
std::expected<OptionalHeader, std::string> optional_header_parse(std::span<const unsigned char> buffer, QWORD optional_header_offset);
