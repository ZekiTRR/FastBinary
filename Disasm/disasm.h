#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "WINAPI/TYPES.h"

// Тонкая RAII-обёртка над Capstone для дизассемблирования x86/x64.
class Disasm {
public:
    enum class Mode {
        X86,
        X64
    };

    struct Instruction {
        QWORD address {};
        std::string bytes;
        std::string mnemonic;
        std::string operands;
    };

    [[nodiscard]]
    static std::expected<Disasm, std::string> create(Mode mode);

    // Дизассемблирует не больше max_instructions инструкций из буфера.
    // address - виртуальный адрес первого байта, он попадает в листинг.
    [[nodiscard]]
    std::expected<std::vector<Instruction>, std::string> disassemble(
        std::span<const unsigned char> code,
        QWORD address,
        size_t max_instructions = 256) const;

    ~Disasm();

    Disasm(Disasm&& other) noexcept;
    Disasm& operator=(Disasm&& other) noexcept;

    Disasm(const Disasm&) = delete;
    Disasm& operator=(const Disasm&) = delete;

private:
    Disasm() = default;

    void* handle_ { nullptr };
};
