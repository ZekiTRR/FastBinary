#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

#include "WINAPI/TYPES.h"

class PE {

public:

    // Битность образа по Magic из OptionalHeader: 0x010B - PE32 (32 бита), 0x020B - PE32+ (64 бита)
    enum class Format {
        Unknown,
        PE32,
        PE32Plus
    };

    enum class Architecture {
        Unknown,
        X86,
        X64,
        Arm,
        ArmThumb2,
        Arm64,
        Itanium
    };

    struct Section {
        std::string name;
        DWORD virtual_size {};
        DWORD virtual_address {};
        DWORD raw_size {};
        DWORD raw_offset {};
        DWORD characteristics {};
        double entropy {};
    };

    struct ImportFunction {
        std::string name;
        WORD ordinal {};
        bool by_ordinal {};
    };

    struct ImportLibrary {
        std::string name;
        std::vector<ImportFunction> functions;
    };

    struct Resource {
        std::string type;
        std::string name;
        DWORD language {};
        DWORD size {};
        QWORD offset {};
    };

    struct TlsInfo {
        bool present {};
        QWORD start_raw_data {};
        QWORD end_raw_data {};
        QWORD address_of_index {};
        QWORD address_of_callbacks {};
        DWORD zero_fill {};
        DWORD characteristics {};
        std::vector<QWORD> callbacks;
    };

    struct StringEntry {
        QWORD offset {};
        std::string text;
    };

    [[nodiscard]]
    static std::expected<PE, std::string> create(std::span<const unsigned char> buffer) {
        PE pe;
        if (auto result = pe.structure_parse(buffer); !result) {
            return std::unexpected(result.error());
        }
        return pe;
    }

    [[nodiscard]]
    static double calculateEntropy(std::span<const unsigned char> data) noexcept;

    // Извлекает ASCII и UTF-16LE строки длиной не меньше min_length.
    [[nodiscard]]
    static std::vector<StringEntry> extractStrings(std::span<const unsigned char> buffer, size_t min_length = 4);

private:
    PE() = default;

    std::expected<void, std::string> structure_parse(std::span<const unsigned char> buffer);
    void sections_parse(std::span<const unsigned char> buffer, QWORD optional_header_offset);
    void imports_parse(std::span<const unsigned char> buffer);
    void tls_parse(std::span<const unsigned char> buffer);
    void resources_parse(std::span<const unsigned char> buffer);

    [[nodiscard]] const IMAGE_DATA_DIRECTORY* data_directory(size_t index) const noexcept;
    [[nodiscard]] Architecture architecture_from_machine(WORD machine) const noexcept;
    [[nodiscard]] std::string_view architecture_to_string() const noexcept;

    _IMAGE_DOS_HEADER dos_header_ {};
    _IMAGE_FILE_HEADER file_header_ {};
    IMAGE_OPTIONAL_HEADER32 optional_header32_ {};
    _IMAGE_OPTIONAL_HEADER64 optional_header64_ {};

    Format format_ { Format::Unknown };
    Architecture architecture_ { Architecture::Unknown };
    QWORD nt_header_offset_ { 0 };
    QWORD base_address_ { 0 };
    DWORD entry_point_rva_ { 0 };
    DWORD subsystem_ { 0 };
    DWORD size_of_headers_ { 0 };
    std::string BuildDateTime_ {};
    std::vector<Section> sections_;
    std::vector<ImportLibrary> imports_;
    std::vector<Resource> resources_;
    TlsInfo tls_ {};

public:
    [[nodiscard]] Format format() const noexcept { return format_; }
    [[nodiscard]] bool b_IsPE64() const noexcept { return format_ == Format::PE32Plus; }
    [[nodiscard]] Architecture e_getArchitecture() const noexcept { return architecture_; }
    [[nodiscard]] std::string_view getArchitectureName() const noexcept { return architecture_to_string(); }
    [[nodiscard]] QWORD qw_getBaseAddress() const noexcept { return base_address_; }
    [[nodiscard]] QWORD qw_getNtHeaderAddress() const noexcept { return nt_header_offset_; }
    [[nodiscard]] const std::string& getBuildDateTime() const noexcept { return BuildDateTime_; }
    [[nodiscard]] DWORD getEntryPointRva() const noexcept { return entry_point_rva_; }
    [[nodiscard]] DWORD getSubsystem() const noexcept { return subsystem_; }
    [[nodiscard]] const std::vector<Section>& getSections() const noexcept { return sections_; }
    [[nodiscard]] const std::vector<ImportLibrary>& getImports() const noexcept { return imports_; }
    [[nodiscard]] const std::vector<Resource>& getResources() const noexcept { return resources_; }
    [[nodiscard]] const TlsInfo& getTls() const noexcept { return tls_; }

    [[nodiscard]] std::optional<QWORD> rvaToOffset(QWORD rva) const noexcept;
    [[nodiscard]] std::optional<QWORD> entryPointOffset() const noexcept;
};
