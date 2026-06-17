#include "zenith/emulator.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <algorithm>
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

bool Emulator::read_memory(std::uint64_t address, std::size_t width, std::uint64_t& value) const noexcept {
    bool ok = false;
    value = read_little_endian(memory, address, width, ok);
    if (ok) return true;

    if (address >= kFramebufferPixelBase) {
        const auto offset = address - kFramebufferPixelBase;
        if (width > framebuffer.size() || offset > framebuffer.size() - width) return false;

        value = 0;
        const auto start = static_cast<std::size_t>(offset);
        for (std::size_t i = 0; i < width; ++i) {
            value |= static_cast<std::uint64_t>(framebuffer[start + i]) << (i * 8U);
        }
        return true;
    }

    const auto read_framebuffer_register =
        [address, width](std::uint64_t register_address, std::uint32_t register_value, std::uint64_t& output) noexcept {
            if (address < register_address) return false;

            const auto offset = address - register_address;
            if (offset > 4 || width > 4 || offset > 4 - width) return false;

            output = (static_cast<std::uint64_t>(register_value) >> (offset * 8U)) & ((1ULL << (width * 8U)) - 1ULL);
            return true;
        };

    const auto resolution =
        static_cast<std::uint32_t>(kFramebufferWidth) | (static_cast<std::uint32_t>(kFramebufferHeight) << 16U);
    if (read_framebuffer_register(kFramebufferResolutionAddress, resolution, value)) return true;

    const auto status = framebuffer_enabled ? kFramebufferEnableBit : 0U;
    return read_framebuffer_register(kFramebufferStatusControlAddress, status, value);
}

bool Emulator::write_memory(std::uint64_t address, std::size_t width, std::uint64_t value) noexcept {
    if (write_little_endian(memory, address, width, value)) return true;

    if (address >= kFramebufferPixelBase) {
        const auto offset = address - kFramebufferPixelBase;
        if (width > framebuffer.size() || offset > framebuffer.size() - width) return false;

        const auto start = static_cast<std::size_t>(offset);
        for (std::size_t i = 0; i < width; ++i) {
            framebuffer[start + i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
        }
        return true;
    }

    if (address <= kFramebufferStatusControlAddress && width <= 8) {
        const auto last_byte = address + width - 1;
        if (last_byte >= kFramebufferStatusControlAddress && last_byte < kFramebufferStatusControlAddress + 4) {
            const auto register_byte_offset = kFramebufferStatusControlAddress - address;
            const auto control_byte = static_cast<std::uint8_t>((value >> (register_byte_offset * 8U)) & 0xFFU);
            framebuffer_enabled = (control_byte & kFramebufferEnableBit) != 0;
            return true;
        }
    }

    return false;
}

void Emulator::reset() {
    pc = 0;
    registers.fill(0);
    memory.fill(0);
    framebuffer.fill(0);
    framebuffer_enabled = false;
}

bool Emulator::load_data(const std::vector<std::uint8_t>& data) noexcept {
    if (data.size() > kInitialStackPointer || data.size() > memory.size()) {
        return false;
    }

    std::copy(data.begin(), data.end(), memory.begin());
    return true;
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
        std::uint64_t value = 0;
        ok = read_memory(address, width, value);
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
    case Operator::BitNot: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        write_register_bits(rd, ~read_register_bits(rs1));
        break;
    }
    case Operator::ShiftLeftLogical:
    case Operator::ShiftLeftArithmetic: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        const auto amount = read_register_bits(rs2) & 0x3FU;
        write_register_bits(rd, read_register_bits(rs1) << amount);
        break;
    }
    case Operator::ShiftRightLogical: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        const auto amount = read_register_bits(rs2) & 0x3FU;
        write_register_bits(rd, read_register_bits(rs1) >> amount);
        break;
    }
    case Operator::ShiftRightArithmetic: {
        const auto rd = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        const auto rs2 = decode_field_3(instruction);
        const auto amount = read_register_bits(rs2) & 0x3FU;
        write_register_signed(rd, read_register_signed(rs1) >> amount);
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
        static_cast<void>(
            write_memory(add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 1, read_register_bits(rs2))
        );
        break;
    }
    case Operator::S16: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(
            write_memory(add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 2, read_register_bits(rs2))
        );
        break;
    }
    case Operator::S32: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(
            write_memory(add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 4, read_register_bits(rs2))
        );
        break;
    }
    case Operator::S64: {
        const auto rs2 = decode_field_1(instruction);
        const auto rs1 = decode_field_2(instruction);
        static_cast<void>(
            write_memory(add_offset(read_register_bits(rs1), decode_imm_15(instruction)), 8, read_register_bits(rs2))
        );
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
        if (read_register_signed(rs1) > read_register_signed(rs2)) next_pc = add_offset(pc, decode_imm_15(instruction));
        break;
    }
    case Operator::Blt: {
        const auto rs1 = decode_field_1(instruction);
        const auto rs2 = decode_field_2(instruction);
        if (read_register_signed(rs1) < read_register_signed(rs2)) next_pc = add_offset(pc, decode_imm_15(instruction));
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

const std::array<std::uint8_t, Emulator::kFramebufferSize>& Emulator::get_framebuffer() const noexcept {
    return framebuffer;
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

emscripten::val get_framebuffer(zenith::emulator::Emulator& emulator) {
    const auto& framebuffer = emulator.get_framebuffer();
    return emscripten::val(emscripten::typed_memory_view(framebuffer.size(), framebuffer.data()));
}

bool load_data(zenith::emulator::Emulator& emulator, emscripten::val input) {
    const auto length = input["length"].as<std::size_t>();
    std::vector<std::uint8_t> data(length);
    for (std::size_t i = 0; i < length; ++i) {
        data[i] = input[i].as<std::uint8_t>();
    }
    return emulator.load_data(data);
}

std::string version() {
    return std::string(zenith::emulator::Emulator::version());
}

EMSCRIPTEN_BINDINGS(zenith_emulator) {
    emscripten::class_<zenith::emulator::Emulator>("Emulator")
        .constructor<>()
        .function("reset", &zenith::emulator::Emulator::reset)
        .function("loadData", &load_data)
        .function("step", &zenith::emulator::Emulator::step)
        .function("getRegisters", &get_registers)
        .function("getFramebuffer", &get_framebuffer)
        .class_function("version", &version);
}

#endif // __EMSCRIPTEN__
