#include "pe.h"

#include <cstring>
#include <format>
#include <string>


// Безопасное чтение структуры T из буфера по оффсету с проверкой границ.
// memcpy вместо reinterpret_cast - данные в буфере не выровнены, а разыменование
// невыровненного указателя - UB.
template <typename T>
static std::expected<T, std::string> read_structure(const std::vector<unsigned char>& buffer, const QWORD offset) {
    if (offset > buffer.size() || sizeof(T) > buffer.size() - offset) {
        return std::unexpected("Buffer is too small: no room for a structure at offset " + std::to_string(offset));
    }

    T value {};
    std::memcpy(&value, buffer.data() + offset, sizeof(T));
    return value;
}


std::expected<void, std::string> PE::structure_parse(const Memory& memory) {
	// 1. Файл должен быть загружен в буфер
	if (!memory.isLoaded()) {
		return std::unexpected("Memory buffer is not loaded: " + memory.loadError().value_or("unknown error"));
	}

	// Создаётся указатель, по идеи нет копирования
	const auto& buffer = memory.getBuffer();

	// 2. Размер файла должен вмещать DOS-заголовок
	if (buffer.size() < sizeof(_IMAGE_DOS_HEADER)) {
		return std::unexpected("File is too small to contain a DOS header.");
	}

	// 3. Читаем DOS-заголовок и проверяем подпись 'MZ'
	const auto dos_header = read_structure<_IMAGE_DOS_HEADER>(buffer, 0);
	if (!dos_header) { // Если T - то значение истинно, если std::string - то ложь
		return std::unexpected(dos_header.error());
	}
	dos_header_ = *dos_header;
	// Т.к dos_header_ - указатель на начало структуры, то теперь ей присваевается начало буфера (который является структурой _IMAGE_DOS_HEADER)
	//																			   и теперь можно обращаться к полям структуры через dos_header_.

	if (dos_header_.e_magic != IMAGE_DOS_SIGNATURE) {
		return std::unexpected("Invalid DOS signature ('MZ' expected).");
	}

	// 4. e_lfanew: оффсет NT-заголовков, не может быть отрицательным
	//    или указывать внутрь DOS-заголовка
	if (dos_header_.e_lfanew < 0 || static_cast<QWORD>(dos_header_.e_lfanew) < sizeof(_IMAGE_DOS_HEADER)) {
		return std::unexpected("Invalid e_lfanew: NT headers overlap the DOS header.");
	}
	nt_header_offset_ = static_cast<QWORD>(dos_header_.e_lfanew);

	// 5. Проверяем подпись NT-заголовков 'PE\0\0'
	const auto nt_signature = read_structure<DWORD>(buffer, nt_header_offset_);
	if (!nt_signature) {
		return std::unexpected(nt_signature.error());
	}
	if (*nt_signature != IMAGE_NT_SIGNATURE) {
		return std::unexpected("Invalid NT signature ('PE\\0\\0' expected) at offset " + std::to_string(nt_header_offset_));
	}

	// 6. FileHeader находится сразу за подписью
	const auto file_header = read_structure<_IMAGE_FILE_HEADER>(buffer, nt_header_offset_ + sizeof(DWORD));
	if (!file_header) {
		return std::unexpected(file_header.error());
	}
	file_header_ = *file_header;

	// 7. Magic из OptionalHeader определяет битность образа
	const QWORD optional_header_offset = nt_header_offset_ + sizeof(DWORD) + sizeof(_IMAGE_FILE_HEADER);
	const auto magic = read_structure<WORD>(buffer, optional_header_offset);
	if (!magic) {
		return std::unexpected(magic.error());
	}

	switch (*magic) {
		// 0x010B - PE32 (32 бита)
		case 0x010B: {
			const auto optional_header = read_structure<IMAGE_OPTIONAL_HEADER32>(buffer, optional_header_offset);
			if (!optional_header) {
				return std::unexpected(optional_header.error());
			}
			format_ = Format::PE32;
			optional_header32_ = *optional_header;
			base_address_ = optional_header32_.ImageBase;
			break;
		}

		// 0x020B - PE32+ (64 бита)
		case 0x020B: {
			const auto optional_header = read_structure<_IMAGE_OPTIONAL_HEADER64>(buffer, optional_header_offset);
			if (!optional_header) {
				return std::unexpected(optional_header.error());
			}
			format_ = Format::PE32Plus;
			optional_header64_ = *optional_header;
			base_address_ = optional_header64_.ImageBase;
			break;
		}

		default:
			return std::unexpected(std::format("Unsupported OptionalHeader magic: 0x{:04X}", *magic));
	}

	return {};
}
