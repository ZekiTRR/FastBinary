#include "pe.h"
#include "structures.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace {

template <typename T>
bool read_at(std::span<const unsigned char> buffer, const QWORD offset, T& value) {
    if (offset > buffer.size() || sizeof(T) > buffer.size() - offset) {
        return false;
    }
    std::memcpy(&value, buffer.data() + offset, sizeof(T));
    return true;
}

std::string read_c_string(std::span<const unsigned char> buffer, const QWORD offset, const size_t max_length = 4096) {
    if (offset >= buffer.size()) {
        return {};
    }
    std::string result;
    const size_t limit = std::min(max_length, buffer.size() - static_cast<size_t>(offset));
    for (size_t i = 0; i < limit; ++i) {
        const char c = static_cast<char>(buffer[offset + i]);
        if (c == '\0') {
            break;
        }
        result += c;
    }
    return result;
}

std::string utf16_to_utf8(std::span<const unsigned char> buffer, const QWORD offset, const size_t characters) {
    if (characters == 0 || characters > 4096 || offset > buffer.size() || characters * 2 > buffer.size() - offset) {
        return {};
    }
    std::wstring wide;
    wide.reserve(characters);
    for (size_t i = 0; i < characters; ++i) {
        wchar_t c {};
        std::memcpy(&c, buffer.data() + offset + i * 2, sizeof(c));
        wide += c;
    }
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string resource_type_name(const DWORD id) {
    switch (id) {
        case 1: return "RT_CURSOR";
        case 2: return "RT_BITMAP";
        case 3: return "RT_ICON";
        case 4: return "RT_MENU";
        case 5: return "RT_DIALOG";
        case 6: return "RT_STRING";
        case 7: return "RT_FONTDIR";
        case 8: return "RT_FONT";
        case 9: return "RT_ACCELERATOR";
        case 10: return "RT_RCDATA";
        case 11: return "RT_MESSAGETABLE";
        case 12: return "RT_GROUP_CURSOR";
        case 14: return "RT_GROUP_ICON";
        case 16: return "RT_VERSION";
        case 24: return "RT_MANIFEST";
        default: return std::format("TYPE_{}", id);
    }
}

bool printable_ascii(const unsigned char c) {
    return c >= 0x20 && c <= 0x7E;
}

}

std::expected<void, std::string> PE::structure_parse(std::span<const unsigned char> buffer) {
    const auto dos_header = dos_header_parse(buffer);
    if (!dos_header) {
        return std::unexpected(dos_header.error());
    }
    dos_header_ = *dos_header;
    nt_header_offset_ = static_cast<QWORD>(dos_header_.e_lfanew);

    if (const auto nt_signature = nt_signature_check(buffer, nt_header_offset_); !nt_signature) {
        return std::unexpected(nt_signature.error());
    }

    const auto file_header = file_header_parse(buffer, nt_header_offset_);
    if (!file_header) {
        return std::unexpected(file_header.error());
    }
    file_header_ = *file_header;
    architecture_ = architecture_from_machine(file_header_.Machine);

    const std::chrono::sys_seconds build_time { std::chrono::seconds { file_header_.TimeDateStamp } };
    BuildDateTime_ = std::format("{:%Y-%m-%d %H:%M:%S} UTC", build_time);

    const QWORD optional_header_offset = nt_header_offset_ + sizeof(DWORD) + sizeof(_IMAGE_FILE_HEADER);
    const auto optional_header = optional_header_parse(buffer, optional_header_offset);
    if (!optional_header) {
        return std::unexpected(optional_header.error());
    }

    if (const auto header32 = std::get_if<IMAGE_OPTIONAL_HEADER32>(&*optional_header)) {
        format_ = Format::PE32;
        optional_header32_ = *header32;
        base_address_ = header32->ImageBase;
        entry_point_rva_ = header32->AddressOfEntryPoint;
        subsystem_ = header32->Subsystem;
        size_of_headers_ = header32->SizeOfHeaders;
    } else {
        const auto& header64 = std::get<_IMAGE_OPTIONAL_HEADER64>(*optional_header);
        format_ = Format::PE32Plus;
        optional_header64_ = header64;
        base_address_ = header64.ImageBase;
        entry_point_rva_ = header64.AddressOfEntryPoint;
        subsystem_ = header64.Subsystem;
        size_of_headers_ = header64.SizeOfHeaders;
    }

    sections_parse(buffer, optional_header_offset);
    imports_parse(buffer);
    tls_parse(buffer);
    resources_parse(buffer);
    return {};
}

const IMAGE_DATA_DIRECTORY* PE::data_directory(const size_t index) const noexcept {
    if (format_ == Format::PE32) {
        return index < optional_header32_.NumberOfRvaAndSizes && index < IMAGE_NUMBEROF_DIRECTORY_ENTRIES
            ? &optional_header32_.DataDirectory[index] : nullptr;
    }
    if (format_ == Format::PE32Plus) {
        return index < optional_header64_.NumberOfRvaAndSizes && index < IMAGE_NUMBEROF_DIRECTORY_ENTRIES
            ? &optional_header64_.DataDirectory[index] : nullptr;
    }
    return nullptr;
}

void PE::sections_parse(std::span<const unsigned char> buffer, const QWORD optional_header_offset) {
    const QWORD table_offset = optional_header_offset + file_header_.SizeOfOptionalHeader;
    const size_t count = std::min<size_t>(file_header_.NumberOfSections, 96);
    sections_.clear();
    sections_.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        IMAGE_SECTION_HEADER header {};
        if (!read_at(buffer, table_offset + i * sizeof(IMAGE_SECTION_HEADER), header)) {
            break;
        }
        Section section;
        size_t name_length = 0;
        while (name_length < IMAGE_SIZEOF_SHORT_NAME && header.Name[name_length] != '\0') {
            ++name_length;
        }
        section.name.assign(reinterpret_cast<const char*>(header.Name), name_length);
        section.virtual_size = header.Misc.VirtualSize;
        section.virtual_address = header.VirtualAddress;
        section.raw_size = header.SizeOfRawData;
        section.raw_offset = header.PointerToRawData;
        section.characteristics = header.Characteristics;
        if (section.raw_offset < buffer.size() && section.raw_size > 0) {
            const size_t available = buffer.size() - section.raw_offset;
            const size_t size = std::min<size_t>(section.raw_size, available);
            section.entropy = calculateEntropy(buffer.subspan(section.raw_offset, size));
        }
        sections_.push_back(std::move(section));
    }
}

std::optional<QWORD> PE::rvaToOffset(const QWORD rva) const noexcept {
    if (rva < size_of_headers_) {
        return rva;
    }
    for (const Section& section : sections_) {
        const QWORD start = section.virtual_address;
        const QWORD span = std::max(section.virtual_size, section.raw_size);
        if (rva >= start && rva < start + span) {
            const QWORD delta = rva - start;
            if (delta < section.raw_size) {
                return section.raw_offset + delta;
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<QWORD> PE::entryPointOffset() const noexcept {
    return rvaToOffset(entry_point_rva_);
}

void PE::imports_parse(std::span<const unsigned char> buffer) {
    imports_.clear();
    const IMAGE_DATA_DIRECTORY* directory = data_directory(IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (!directory || directory->VirtualAddress == 0) {
        return;
    }
    const auto descriptor_offset = rvaToOffset(directory->VirtualAddress);
    if (!descriptor_offset) {
        return;
    }

    for (size_t index = 0; index < 4096; ++index) {
        IMAGE_IMPORT_DESCRIPTOR descriptor {};
        if (!read_at(buffer, *descriptor_offset + index * sizeof(IMAGE_IMPORT_DESCRIPTOR), descriptor)) {
            break;
        }
        if (descriptor.Name == 0 && descriptor.FirstThunk == 0 && descriptor.OriginalFirstThunk == 0) {
            break;
        }
        const auto name_offset = rvaToOffset(descriptor.Name);
        if (!name_offset) {
            continue;
        }

        ImportLibrary library;
        library.name = read_c_string(buffer, *name_offset);
        const DWORD thunk_rva = descriptor.OriginalFirstThunk != 0 ? descriptor.OriginalFirstThunk : descriptor.FirstThunk;
        const auto thunk_offset = rvaToOffset(thunk_rva);
        if (!thunk_offset) {
            imports_.push_back(std::move(library));
            continue;
        }

        const size_t thunk_size = b_IsPE64() ? sizeof(QWORD) : sizeof(DWORD);
        for (size_t thunk_index = 0; thunk_index < 16384; ++thunk_index) {
            QWORD thunk_value = 0;
            if (b_IsPE64()) {
                if (!read_at(buffer, *thunk_offset + thunk_index * thunk_size, thunk_value)) {
                    break;
                }
            } else {
                DWORD value = 0;
                if (!read_at(buffer, *thunk_offset + thunk_index * thunk_size, value)) {
                    break;
                }
                thunk_value = value;
            }
            if (thunk_value == 0) {
                break;
            }

            ImportFunction function;
            const QWORD ordinal_flag = b_IsPE64() ? IMAGE_ORDINAL_FLAG64 : IMAGE_ORDINAL_FLAG32;
            if ((thunk_value & ordinal_flag) != 0) {
                function.by_ordinal = true;
                function.ordinal = static_cast<WORD>(thunk_value & 0xFFFF);
            } else {
                const auto import_name_offset = rvaToOffset(thunk_value & 0x7FFFFFFF);
                if (import_name_offset) {
                    function.name = read_c_string(buffer, *import_name_offset + sizeof(WORD));
                }
            }
            library.functions.push_back(std::move(function));
        }
        imports_.push_back(std::move(library));
    }
}

void PE::tls_parse(std::span<const unsigned char> buffer) {
    tls_ = {};
    const IMAGE_DATA_DIRECTORY* directory = data_directory(IMAGE_DIRECTORY_ENTRY_TLS);
    if (!directory || directory->VirtualAddress == 0) {
        return;
    }
    const auto offset = rvaToOffset(directory->VirtualAddress);
    if (!offset) {
        return;
    }

    tls_.present = true;
    QWORD callbacks_va = 0;
    if (b_IsPE64()) {
        IMAGE_TLS_DIRECTORY64 tls {};
        if (!read_at(buffer, *offset, tls)) {
            tls_ = {};
            return;
        }
        tls_.start_raw_data = tls.StartAddressOfRawData;
        tls_.end_raw_data = tls.EndAddressOfRawData;
        tls_.address_of_index = tls.AddressOfIndex;
        tls_.address_of_callbacks = tls.AddressOfCallBacks;
        tls_.zero_fill = tls.SizeOfZeroFill;
        tls_.characteristics = tls.Characteristics;
        callbacks_va = tls.AddressOfCallBacks;
    } else {
        IMAGE_TLS_DIRECTORY32 tls {};
        if (!read_at(buffer, *offset, tls)) {
            tls_ = {};
            return;
        }
        tls_.start_raw_data = tls.StartAddressOfRawData;
        tls_.end_raw_data = tls.EndAddressOfRawData;
        tls_.address_of_index = tls.AddressOfIndex;
        tls_.address_of_callbacks = tls.AddressOfCallBacks;
        tls_.zero_fill = tls.SizeOfZeroFill;
        tls_.characteristics = tls.Characteristics;
        callbacks_va = tls.AddressOfCallBacks;
    }

    if (callbacks_va < base_address_) {
        return;
    }
    const auto callbacks_offset = rvaToOffset(callbacks_va - base_address_);
    if (!callbacks_offset) {
        return;
    }
    for (size_t i = 0; i < 1024; ++i) {
        QWORD callback = 0;
        if (b_IsPE64()) {
            if (!read_at(buffer, *callbacks_offset + i * sizeof(QWORD), callback)) {
                break;
            }
        } else {
            DWORD value = 0;
            if (!read_at(buffer, *callbacks_offset + i * sizeof(DWORD), value)) {
                break;
            }
            callback = value;
        }
        if (callback == 0) {
            break;
        }
        tls_.callbacks.push_back(callback);
    }
}

void PE::resources_parse(std::span<const unsigned char> buffer) {
    resources_.clear();
    const IMAGE_DATA_DIRECTORY* directory = data_directory(IMAGE_DIRECTORY_ENTRY_RESOURCE);
    if (!directory || directory->VirtualAddress == 0) {
        return;
    }
    const QWORD root_rva = directory->VirtualAddress;
    const auto root_offset = rvaToOffset(root_rva);
    if (!root_offset) {
        return;
    }

    std::function<void(QWORD, const std::string&, const std::string&, DWORD, size_t)> visit;
    visit = [&](const QWORD directory_rva, const std::string& type, const std::string& name, const DWORD language, const size_t level) {
        if (level > 2 || resources_.size() >= 2048) {
            return;
        }
        const auto current_offset = rvaToOffset(directory_rva);
        if (!current_offset) {
            return;
        }
        IMAGE_RESOURCE_DIRECTORY resource_directory {};
        if (!read_at(buffer, *current_offset, resource_directory)) {
            return;
        }
        const size_t count = std::min<size_t>(resource_directory.NumberOfNamedEntries + resource_directory.NumberOfIdEntries, 2048);
        for (size_t i = 0; i < count; ++i) {
            IMAGE_RESOURCE_DIRECTORY_ENTRY entry {};
            if (!read_at(buffer, *current_offset + sizeof(IMAGE_RESOURCE_DIRECTORY) + i * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY), entry)) {
                break;
            }
            std::string entry_name;
            if ((entry.Name & IMAGE_RESOURCE_NAME_IS_STRING) != 0) {
                const auto string_offset = rvaToOffset(root_rva + (entry.Name & 0x7FFFFFFF));
                if (string_offset) {
                    WORD length = 0;
                    if (read_at(buffer, *string_offset, length)) {
                        entry_name = utf16_to_utf8(buffer, *string_offset + sizeof(WORD), length);
                    }
                }
            } else {
                const DWORD id = entry.Name & 0xFFFF;
                entry_name = level == 0 ? resource_type_name(id) : std::format("#{}", id);
            }

            if ((entry.OffsetToData & IMAGE_RESOURCE_DATA_IS_DIRECTORY) != 0) {
                const DWORD next_language = level == 1 ? entry.Name & 0xFFFF : language;
                visit(root_rva + (entry.OffsetToData & 0x7FFFFFFF),
                    level == 0 ? entry_name : type,
                    level == 1 ? entry_name : name,
                    next_language,
                    level + 1);
            } else {
                const auto data_entry_offset = rvaToOffset(root_rva + entry.OffsetToData);
                IMAGE_RESOURCE_DATA_ENTRY data_entry {};
                if (!data_entry_offset || !read_at(buffer, *data_entry_offset, data_entry)) {
                    continue;
                }
                const auto data_offset = rvaToOffset(data_entry.OffsetToData);
                resources_.push_back(Resource {
                    .type = level == 0 ? entry_name : type,
                    .name = level == 1 ? entry_name : name,
                    .language = language,
                    .size = data_entry.Size,
                    .offset = data_offset.value_or(0)
                });
            }
        }
    };
    visit(root_rva, {}, {}, 0, 0);
}

double PE::calculateEntropy(const std::span<const unsigned char> data) noexcept {
    if (data.empty()) {
        return 0.0;
    }
    std::array<size_t, 256> counts {};
    for (const unsigned char byte : data) {
        ++counts[byte];
    }
    double entropy = 0.0;
    for (const size_t count : counts) {
        if (count == 0) {
            continue;
        }
        const double probability = static_cast<double>(count) / static_cast<double>(data.size());
        entropy -= probability * std::log2(probability);
    }
    return entropy;
}

std::vector<PE::StringEntry> PE::extractStrings(const std::span<const unsigned char> buffer, const size_t min_length) {
    std::vector<StringEntry> result;
    if (min_length == 0) {
        return result;
    }
    size_t i = 0;
    while (i < buffer.size() && result.size() < 100000) {
        if (i + 1 < buffer.size() && printable_ascii(buffer[i]) && buffer[i + 1] == 0) {
            const size_t start = i;
            std::string text;
            while (i + 1 < buffer.size() && printable_ascii(buffer[i]) && buffer[i + 1] == 0) {
                text += static_cast<char>(buffer[i]);
                i += 2;
            }
            if (text.size() >= min_length) {
                result.push_back(StringEntry { .offset = start, .text = std::move(text) });
            }
            continue;
        }
        if (printable_ascii(buffer[i])) {
            const size_t start = i;
            while (i < buffer.size() && printable_ascii(buffer[i])) {
                ++i;
            }
            if (i - start >= min_length) {
                result.push_back(StringEntry {
                    .offset = start,
                    .text = std::string(reinterpret_cast<const char*>(buffer.data() + start), i - start)
                });
            }
            continue;
        }
        ++i;
    }
    return result;
}

PE::Architecture PE::architecture_from_machine(const WORD machine) const noexcept {
    switch (machine) {
        case IMAGE_FILE_MACHINE_I386: return Architecture::X86;
        case IMAGE_FILE_MACHINE_AMD64: return Architecture::X64;
        case IMAGE_FILE_MACHINE_ARM: return Architecture::Arm;
        case IMAGE_FILE_MACHINE_ARMNT: return Architecture::ArmThumb2;
        case IMAGE_FILE_MACHINE_ARM64: return Architecture::Arm64;
        case IMAGE_FILE_MACHINE_IA64: return Architecture::Itanium;
        default: return Architecture::Unknown;
    }
}

std::string_view PE::architecture_to_string() const noexcept {
    switch (architecture_) {
        case Architecture::Unknown: return "Unknown";
        case Architecture::X86: return "x86";
        case Architecture::X64: return "x64";
        case Architecture::Arm: return "ARM";
        case Architecture::ArmThumb2: return "ARM (Thumb-2)";
        case Architecture::Arm64: return "ARM64";
        case Architecture::Itanium: return "Itanium";
    }
    return "Unknown";
}
