#include "zenith/emulator.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace zenith::emulator {

bool Emulator::is_memory_access_in_bounds(
    const std::array<std::uint8_t, kMemorySize>& memory, std::uint64_t address, std::size_t width
) noexcept {
    const auto memory_size = static_cast<std::uint64_t>(memory.size());

    if (address > memory_size) return false;
    if (width > memory.size()) return false;
    return address <= memory_size - width;
}

std::uint64_t Emulator::read_little_endian(
    const std::array<std::uint8_t, kMemorySize>& memory, std::uint64_t address, std::size_t width, bool& ok
) noexcept {
    ok = is_memory_access_in_bounds(memory, address, width);
    if (!ok) return 0;

    std::uint64_t value = 0;
    const auto start = static_cast<std::size_t>(address);
    for (std::size_t i = 0; i < width; ++i) {
        value |= static_cast<std::uint64_t>(memory[start + i]) << (i * 8U);
    }
    return value;
}

bool Emulator::write_little_endian(
    std::array<std::uint8_t, kMemorySize>& memory, std::uint64_t address, std::size_t width, std::uint64_t value
) noexcept {
    if (!is_memory_access_in_bounds(memory, address, width)) return false;

    const auto start = static_cast<std::size_t>(address);
    for (std::size_t i = 0; i < width; ++i) {
        memory[start + i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
    }
    return true;
}

std::uint64_t Emulator::signed_divide(std::uint64_t lhs_bits, std::uint64_t rhs_bits, bool& ok) noexcept {
    const auto lhs = std::bit_cast<std::int64_t>(lhs_bits);
    const auto rhs = std::bit_cast<std::int64_t>(rhs_bits);
    ok = rhs != 0;

    if (rhs == 0) return 0;
    if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1) return lhs_bits;

    return std::bit_cast<std::uint64_t>(lhs / rhs);
}

void Emulator::reset() {
    pc = 0;
    registers.fill(0);
    memory.fill(0);
}

void Emulator::step(std::uint32_t instruction) {
    const auto read_register_bits = [this](std::uint32_t index) noexcept {
        return std::bit_cast<std::uint64_t>(registers[index]);
    };
    const auto read_register_signed = [this](std::uint32_t index) noexcept { return registers[index]; };
    const auto write_register_bits = [this](std::uint32_t index, std::uint64_t value) noexcept {
        if (index == 0) return;

        registers[index] = std::bit_cast<std::int64_t>(value);
    };
    const auto write_register_signed = [&write_register_bits](std::uint32_t index, std::int64_t value) noexcept {
        write_register_bits(index, std::bit_cast<std::uint64_t>(value));
    };
    const auto load_signed = [this, &write_register_bits, &write_register_signed](
                                 std::uint32_t rd, std::uint64_t address, std::size_t width
                             ) noexcept {
        bool ok = false;
        const auto value = read_little_endian(memory, address, width, ok);
        if (!ok) return;

        switch (width) {
        case 1:
            write_register_signed(rd, static_cast<std::int8_t>(value));
            break;
        case 2:
            write_register_signed(rd, static_cast<std::int16_t>(value));
            break;
        case 4:
            write_register_signed(rd, static_cast<std::int32_t>(value));
            break;
        case 8:
            write_register_bits(rd, value);
            break;
        default:
            break;
        }
    };

    const auto op = static_cast<Operator>(instruction & 0x7FU);
    std::uint64_t next_pc = pc + 4;

    switch (op) {
    case Operator::Add: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        write_register_bits(rd, read_register_bits(rs1) + read_register_bits(rs2));
        break;
    }
    case Operator::Sub: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        write_register_bits(rd, read_register_bits(rs1) - read_register_bits(rs2));
        break;
    }
    case Operator::Mul: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        write_register_bits(rd, read_register_bits(rs1) * read_register_bits(rs2));
        break;
    }
    case Operator::Div: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        bool ok = false;
        const auto quotient = signed_divide(read_register_bits(rs1), read_register_bits(rs2), ok);
        if (ok) write_register_bits(rd, quotient);
        break;
    }
    case Operator::Addi: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto imm = decode_imm_15(instruction);
        write_register_bits(rd, read_register_bits(rs1) + static_cast<std::uint64_t>(imm));
        break;
    }
    case Operator::Muli: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto imm = decode_imm_15(instruction);
        write_register_bits(rd, read_register_bits(rs1) * static_cast<std::uint64_t>(imm));
        break;
    }
    case Operator::Divi: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto imm = decode_imm_15(instruction);
        bool ok = false;
        const auto quotient =
            signed_divide(read_register_bits(rs1), std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(imm)), ok);
        if (ok) write_register_bits(rd, quotient);
        break;
    }
    case Operator::BitAnd: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        write_register_bits(rd, read_register_bits(rs1) & read_register_bits(rs2));
        break;
    }
    case Operator::BitOr: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        write_register_bits(rd, read_register_bits(rs1) | read_register_bits(rs2));
        break;
    }
    case Operator::BitXor: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        write_register_bits(rd, read_register_bits(rs1) ^ read_register_bits(rs2));
        break;
    }
    case Operator::L8: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        load_signed(rd, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 1);
        break;
    }
    case Operator::L16: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        load_signed(rd, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 2);
        break;
    }
    case Operator::L32: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        load_signed(rd, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 4);
        break;
    }
    case Operator::L64: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        load_signed(rd, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 8);
        break;
    }
    case Operator::S8: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(write_little_endian(
            memory, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 1, read_register_bits(rs2)
        ));
        break;
    }
    case Operator::S16: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(write_little_endian(
            memory, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 2, read_register_bits(rs2)
        ));
        break;
    }
    case Operator::S32: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(write_little_endian(
            memory, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 4, read_register_bits(rs2)
        ));
        break;
    }
    case Operator::S64: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(write_little_endian(
            memory, add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 8, read_register_bits(rs2)
        ));
        break;
    }
    case Operator::Beq: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) == read_register_signed(rs2))
            next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Bne: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) != read_register_signed(rs2))
            next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Bge: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) >= read_register_signed(rs2))
            next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Ble: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) <= read_register_signed(rs2))
            next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Bgt: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) > read_register_signed(rs2))
            next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Blt: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) < read_register_signed(rs2))
            next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Jal: {
        const auto rd = decode_field_1(instruction);
        write_register_bits(rd, next_pc);
        next_pc = add_offset(pc, decode_imm_20(instruction));
        break;
    }
    case Operator::Jalr: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        write_register_bits(rd, next_pc);
        next_pc = add_offset(read_register_bits(rs1), decode_imm_15(instruction));
        break;
    }
    default:
        // todo: tensor and system
        break;
    }

    pc = next_pc;
    registers[0] = 0;
}

std::string_view Emulator::version() noexcept {
    return "0.1.0";
}

std::array<int64_t, 32> Emulator::get_registers() {
    return registers;
}

} // namespace zenith::emulator

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

emscripten::val get_registers(zenith::emulator::Emulator& emulator) {
    const auto registers = emulator.get_registers();
    auto result = emscripten::val::array();
    const auto big_int = emscripten::val::global("BigInt");

    for (std::size_t i = 0; i < registers.size(); ++i) {
        result.set(i, big_int(std::to_string(registers[i])));
    }

    return result;
}

std::string version() {
    return std::string(zenith::emulator::Emulator::version());
}

EMSCRIPTEN_BINDINGS(zenith_emulator) {
    emscripten::class_<zenith::emulator::Emulator>("Emulator")
        .constructor<>()
        .function("reset", &zenith::emulator::Emulator::reset)
        .function("step", &zenith::emulator::Emulator::step)
        .function("getRegisters", &get_registers)
        .class_function("version", &version);
}

#endif // __EMSCRIPTEN__
