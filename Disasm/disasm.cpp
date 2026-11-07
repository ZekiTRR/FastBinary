#include "disasm.h"

#include <capstone/capstone.h>

#include <format>
#include <utility>

std::expected<Disasm, std::string> Disasm::create(const Mode mode) {
    Disasm disasm;
    csh handle {};
    const cs_mode cs_mode_value = mode == Mode::X64 ? CS_MODE_64 : CS_MODE_32;
    const cs_err error = cs_open(CS_ARCH_X86, cs_mode_value, &handle);
    if (error != CS_ERR_OK) {
        return std::unexpected(std::string("Capstone: ") + cs_strerror(error));
    }
    disasm.handle_ = new csh(handle);
    return disasm;
}

std::expected<std::vector<Disasm::Instruction>, std::string> Disasm::disassemble(
    const std::span<const unsigned char> code,
    const QWORD address,
    const size_t max_instructions) const {
    if (!handle_) {
        return std::unexpected("Capstone is not initialized.");
    }
    if (code.empty() || max_instructions == 0) {
        return std::vector<Instruction> {};
    }

    const csh handle = *static_cast<csh*>(handle_);
    cs_insn* instructions = nullptr;
    const size_t count = cs_disasm(handle, code.data(), code.size(), address, max_instructions, &instructions);
    if (count == 0) {
        return std::unexpected("Capstone could not disassemble the supplied bytes.");
    }

    std::vector<Instruction> result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const cs_insn& current = instructions[i];
        std::string bytes;
        for (size_t byte_index = 0; byte_index < current.size; ++byte_index) {
            if (byte_index != 0) {
                bytes += ' ';
            }
            bytes += std::format("{:02X}", current.bytes[byte_index]);
        }
        result.push_back(Instruction {
            .address = current.address,
            .bytes = std::move(bytes),
            .mnemonic = current.mnemonic,
            .operands = current.op_str
        });
    }
    cs_free(instructions, count);
    return result;
}

Disasm::~Disasm() {
    if (handle_) {
        csh handle = *static_cast<csh*>(handle_);
        cs_close(&handle);
        delete static_cast<csh*>(handle_);
    }
}

Disasm::Disasm(Disasm&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)) {
}

Disasm& Disasm::operator=(Disasm&& other) noexcept {
    if (this != &other) {
        if (handle_) {
            csh handle = *static_cast<csh*>(handle_);
            cs_close(&handle);
            delete static_cast<csh*>(handle_);
        }
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}
