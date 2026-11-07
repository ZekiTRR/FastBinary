#include "structures.h"

#include <cstring>
#include <format>
#include <string>


// Безопасное чтение структуры T из буфера по оффсету с проверкой границ.
// memcpy вместо reinterpret_cast - данные в буфере не выровнены, а разыменование
// невыровненного указателя - UB.
// static: утилита нужна только внутри этого файла (внутренняя линковка).
template <typename T>
static std::expected<T, std::string> read_structure(std::span<const unsigned char> buffer, const QWORD offset) {
    if (offset > buffer.size() || sizeof(T) > buffer.size() - offset) {
        return std::unexpected("Buffer is too small: no room for a structure at offset " + std::to_string(offset));
    }

    T value {};
    std::memcpy(&value, buffer.data() + offset, sizeof(T));
    return value;
}

std::expected<_IMAGE_DOS_HEADER, std::string> dos_header_parse(std::span<const unsigned char> buffer) {
	// Файл должен вмещать DOS-заголовок
	if (buffer.size() < sizeof(_IMAGE_DOS_HEADER)) {
		return std::unexpected("File is too small to contain a DOS header.");
	}

	const auto dos_header = read_structure<_IMAGE_DOS_HEADER>(buffer, 0);
	if (!dos_header) { // Если T - то значение истинно, если std::string - то ложь
		return std::unexpected(dos_header.error());
	}

	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
		return std::unexpected("Invalid DOS signature ('MZ' expected).");
	}

	// e_lfanew: оффсет NT-заголовков, не может быть отрицательным
	// или указывать внутрь DOS-заголовка
	if (dos_header->e_lfanew < 0 || static_cast<QWORD>(dos_header->e_lfanew) < sizeof(_IMAGE_DOS_HEADER)) {
		return std::unexpected("Invalid e_lfanew: NT headers overlap the DOS header.");
	}

	return *dos_header;
}

std::expected<void, std::string> nt_signature_check(std::span<const unsigned char> buffer, const QWORD nt_header_offset) {
	const auto nt_signature = read_structure<DWORD>(buffer, nt_header_offset);
	if (!nt_signature) {
		return std::unexpected(nt_signature.error());
	}

	if (*nt_signature != IMAGE_NT_SIGNATURE) {
		return std::unexpected("Invalid NT signature ('PE\\0\\0' expected) at offset " + std::to_string(nt_header_offset));
	}

	return {};
}

std::expected<_IMAGE_FILE_HEADER, std::string> file_header_parse(std::span<const unsigned char> buffer, const QWORD nt_header_offset) {
	// FileHeader находится сразу за подписью - 2 поле структуры IMAGE_NT_HEADERS
	return read_structure<_IMAGE_FILE_HEADER>(buffer, nt_header_offset + sizeof(DWORD));
}

std::expected<OptionalHeader, std::string> optional_header_parse(std::span<const unsigned char> buffer, const QWORD optional_header_offset) {
	// Magic - первое поле OptionalHeader, определяет битность образа
	const auto magic = read_structure<WORD>(buffer, optional_header_offset);
	if (!magic) {
		return std::unexpected(magic.error());
	}

	switch (*magic) { // Switch достаёт из std::expected значение, с помощью *
		// 0x010B - PE32 (32 бита)
		case 0x010B: {
			const auto optional_header = read_structure<IMAGE_OPTIONAL_HEADER32>(buffer, optional_header_offset);
			if (!optional_header) {
				return std::unexpected(optional_header.error());
			}
			return *optional_header; // IMAGE_OPTIONAL_HEADER32 -> активная альтернатива variant'а
		}

		// 0x020B - PE32+ (64 бита)
		case 0x020B: {
			const auto optional_header = read_structure<_IMAGE_OPTIONAL_HEADER64>(buffer, optional_header_offset);
			if (!optional_header) {
				return std::unexpected(optional_header.error());
			}
			return *optional_header; // _IMAGE_OPTIONAL_HEADER64 -> активная альтернатива variant'а
		}

		default:
			return std::unexpected(std::format("Unsupported OptionalHeader magic: 0x{:04X}", *magic));
	}
}
