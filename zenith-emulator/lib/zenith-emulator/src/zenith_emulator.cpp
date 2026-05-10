#include "zenith_emulator/zenith_emulator.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace zenith_emulator {
namespace {

constexpr std::uint8_t kOpAdd = 0x00;
constexpr std::uint8_t kOpSub = 0x01;
constexpr std::uint8_t kOpMul = 0x02;
constexpr std::uint8_t kOpDiv = 0x03;
constexpr std::uint8_t kOpAddi = 0x04;
constexpr std::uint8_t kOpMuli = 0x05;
constexpr std::uint8_t kOpDivi = 0x06;
constexpr std::uint8_t kOpAnd = 0x07;
constexpr std::uint8_t kOpOr = 0x08;
constexpr std::uint8_t kOpXor = 0x09;
constexpr std::uint8_t kOpL8 = 0x0A;
constexpr std::uint8_t kOpL16 = 0x0B;
constexpr std::uint8_t kOpL32 = 0x0C;
constexpr std::uint8_t kOpL64 = 0x0D;
constexpr std::uint8_t kOpS8 = 0x0E;
constexpr std::uint8_t kOpS16 = 0x0F;
constexpr std::uint8_t kOpS32 = 0x10;
constexpr std::uint8_t kOpS64 = 0x11;
constexpr std::uint8_t kOpBeq = 0x14;
constexpr std::uint8_t kOpBne = 0x15;
constexpr std::uint8_t kOpBge = 0x16;
constexpr std::uint8_t kOpBle = 0x17;
constexpr std::uint8_t kOpBgt = 0x18;
constexpr std::uint8_t kOpBlt = 0x19;
constexpr std::uint8_t kOpJal = 0x1A;
constexpr std::uint8_t kOpJalr = 0x1B;

constexpr std::uint64_t kInstructionSizeBytes = 4;

constexpr std::uint32_t DecodeField1(std::uint32_t instruction) noexcept {
  return (instruction >> 7U) & 0x1FU;
}

constexpr std::uint32_t DecodeField2(std::uint32_t instruction) noexcept {
  return (instruction >> 12U) & 0x1FU;
}

constexpr std::uint32_t DecodeField3(std::uint32_t instruction) noexcept {
  return (instruction >> 17U) & 0x1FU;
}

constexpr std::int32_t DecodeImm15(std::uint32_t instruction) noexcept {
  return static_cast<std::int32_t>(instruction) >> 17;
}

constexpr std::int32_t DecodeImm20(std::uint32_t instruction) noexcept {
  return static_cast<std::int32_t>(instruction) >> 12;
}

constexpr std::uint64_t AddOffset(std::uint64_t base, std::int32_t offset) noexcept {
  return base + static_cast<std::uint64_t>(offset);
}

template <std::size_t N>
bool IsMemoryAccessInBounds(const std::array<std::uint8_t, N>& memory,
                            std::uint64_t address,
                            std::size_t width) noexcept {
  const auto memory_size = static_cast<std::uint64_t>(memory.size());
  if (address > memory_size) {
    return false;
  }

  if (width > memory.size()) {
    return false;
  }

  return address <= memory_size - width;
}

template <std::size_t N>
std::optional<std::uint64_t> ReadLittleEndian(const std::array<std::uint8_t, N>& memory,
                                              std::uint64_t address,
                                              std::size_t width) noexcept {
  if (!IsMemoryAccessInBounds(memory, address, width)) {
    return std::nullopt;
  }

  std::uint64_t value = 0;
  const auto start = static_cast<std::size_t>(address);
  for (std::size_t i = 0; i < width; ++i) {
    value |= static_cast<std::uint64_t>(memory[start + i]) << (i * 8U);
  }
  return value;
}

template <std::size_t N>
bool WriteLittleEndian(std::array<std::uint8_t, N>& memory,
                       std::uint64_t address,
                       std::size_t width,
                       std::uint64_t value) noexcept {
  if (!IsMemoryAccessInBounds(memory, address, width)) {
    return false;
  }

  const auto start = static_cast<std::size_t>(address);
  for (std::size_t i = 0; i < width; ++i) {
    memory[start + i] = static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU);
  }
  return true;
}

std::optional<std::uint64_t> SignedDivide(std::uint64_t lhs_bits,
                                          std::uint64_t rhs_bits) noexcept {
  const auto lhs = std::bit_cast<std::int64_t>(lhs_bits);
  const auto rhs = std::bit_cast<std::int64_t>(rhs_bits);
  if (rhs == 0) {
    return std::nullopt;
  }

  if (lhs == std::numeric_limits<std::int64_t>::min() && rhs == -1) {
    return lhs_bits;
  }

  return std::bit_cast<std::uint64_t>(lhs / rhs);
}

}  // namespace

void Emulator::Reset() {
  accumulator_ = 0;
  pc = 0;
  registers.fill(0);
  memory.fill(0);
}

void Emulator::Step(std::uint32_t instruction) {
  const auto read_register_bits = [this](std::uint32_t index) noexcept {
    return std::bit_cast<std::uint64_t>(registers[index]);
  };
  const auto read_register_signed = [this](std::uint32_t index) noexcept {
    return registers[index];
  };
  const auto write_register_bits = [this](std::uint32_t index, std::uint64_t value) noexcept {
    if (index == 0) {
      return;
    }

    registers[index] = std::bit_cast<std::int64_t>(value);
  };
  const auto write_register_signed = [&write_register_bits](std::uint32_t index,
                                                            std::int64_t value) noexcept {
    write_register_bits(index, std::bit_cast<std::uint64_t>(value));
  };
  const auto load_signed = [this, &write_register_bits, &write_register_signed](
                               std::uint32_t rd,
                               std::uint64_t address,
                               std::size_t width) noexcept {
    const auto value = ReadLittleEndian(memory, address, width);
    if (!value.has_value()) {
      return;
    }

    switch (width) {
      case 1:
        write_register_signed(rd, static_cast<std::int8_t>(*value));
        break;
      case 2:
        write_register_signed(rd, static_cast<std::int16_t>(*value));
        break;
      case 4:
        write_register_signed(rd, static_cast<std::int32_t>(*value));
        break;
      case 8:
        write_register_bits(rd, *value);
        break;
      default:
        break;
    }
  };

  const std::uint8_t op = instruction & 0x7FU;
  std::uint64_t next_pc = pc + kInstructionSizeBytes;

  switch (op) {
    case kOpAdd: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      write_register_bits(rd, read_register_bits(rs1) + read_register_bits(rs2));
      break;
    }
    case kOpSub: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      write_register_bits(rd, read_register_bits(rs1) - read_register_bits(rs2));
      break;
    }
    case kOpMul: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      write_register_bits(rd, read_register_bits(rs1) * read_register_bits(rs2));
      break;
    }
    case kOpDiv: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      const auto quotient = SignedDivide(read_register_bits(rs1), read_register_bits(rs2));
      if (quotient.has_value()) {
        write_register_bits(rd, *quotient);
      }
      break;
    }
    case kOpAddi: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto imm = DecodeImm15(instruction);
      write_register_bits(rd, read_register_bits(rs1) + static_cast<std::uint64_t>(imm));
      break;
    }
    case kOpMuli: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto imm = DecodeImm15(instruction);
      write_register_bits(rd, read_register_bits(rs1) * static_cast<std::uint64_t>(imm));
      break;
    }
    case kOpDivi: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto imm = DecodeImm15(instruction);
      const auto quotient = SignedDivide(
          read_register_bits(rs1),
          std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(imm)));
      if (quotient.has_value()) {
        write_register_bits(rd, *quotient);
      }
      break;
    }
    case kOpAnd: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      write_register_bits(rd, read_register_bits(rs1) & read_register_bits(rs2));
      break;
    }
    case kOpOr: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      write_register_bits(rd, read_register_bits(rs1) | read_register_bits(rs2));
      break;
    }
    case kOpXor: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      const auto rs2 = DecodeField3(instruction);
      write_register_bits(rd, read_register_bits(rs1) ^ read_register_bits(rs2));
      break;
    }
    case kOpL8: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      load_signed(rd, AddOffset(read_register_bits(rs1), DecodeImm15(instruction)), 1);
      break;
    }
    case kOpL16: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      load_signed(rd, AddOffset(read_register_bits(rs1), DecodeImm15(instruction)), 2);
      break;
    }
    case kOpL32: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      load_signed(rd, AddOffset(read_register_bits(rs1), DecodeImm15(instruction)), 4);
      break;
    }
    case kOpL64: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      load_signed(rd, AddOffset(read_register_bits(rs1), DecodeImm15(instruction)), 8);
      break;
    }
    case kOpS8: {
      const auto rs2 = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      WriteLittleEndian(memory,
                        AddOffset(read_register_bits(rs1), DecodeImm15(instruction)),
                        1,
                        read_register_bits(rs2));
      break;
    }
    case kOpS16: {
      const auto rs2 = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      WriteLittleEndian(memory,
                        AddOffset(read_register_bits(rs1), DecodeImm15(instruction)),
                        2,
                        read_register_bits(rs2));
      break;
    }
    case kOpS32: {
      const auto rs2 = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      WriteLittleEndian(memory,
                        AddOffset(read_register_bits(rs1), DecodeImm15(instruction)),
                        4,
                        read_register_bits(rs2));
      break;
    }
    case kOpS64: {
      const auto rs2 = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      WriteLittleEndian(memory,
                        AddOffset(read_register_bits(rs1), DecodeImm15(instruction)),
                        8,
                        read_register_bits(rs2));
      break;
    }
    case kOpBeq: {
      const auto rs1 = DecodeField1(instruction);
      const auto rs2 = DecodeField2(instruction);
      if (read_register_signed(rs1) == read_register_signed(rs2)) {
        next_pc = AddOffset(pc, DecodeImm15(instruction));
      }
      break;
    }
    case kOpBne: {
      const auto rs1 = DecodeField1(instruction);
      const auto rs2 = DecodeField2(instruction);
      if (read_register_signed(rs1) != read_register_signed(rs2)) {
        next_pc = AddOffset(pc, DecodeImm15(instruction));
      }
      break;
    }
    case kOpBge: {
      const auto rs1 = DecodeField1(instruction);
      const auto rs2 = DecodeField2(instruction);
      if (read_register_signed(rs1) >= read_register_signed(rs2)) {
        next_pc = AddOffset(pc, DecodeImm15(instruction));
      }
      break;
    }
    case kOpBle: {
      const auto rs1 = DecodeField1(instruction);
      const auto rs2 = DecodeField2(instruction);
      if (read_register_signed(rs1) <= read_register_signed(rs2)) {
        next_pc = AddOffset(pc, DecodeImm15(instruction));
      }
      break;
    }
    case kOpBgt: {
      const auto rs1 = DecodeField1(instruction);
      const auto rs2 = DecodeField2(instruction);
      if (read_register_signed(rs1) > read_register_signed(rs2)) {
        next_pc = AddOffset(pc, DecodeImm15(instruction));
      }
      break;
    }
    case kOpBlt: {
      const auto rs1 = DecodeField1(instruction);
      const auto rs2 = DecodeField2(instruction);
      if (read_register_signed(rs1) < read_register_signed(rs2)) {
        next_pc = AddOffset(pc, DecodeImm15(instruction));
      }
      break;
    }
    case kOpJal: {
      const auto rd = DecodeField1(instruction);
      write_register_bits(rd, next_pc);
      next_pc = AddOffset(pc, DecodeImm20(instruction));
      break;
    }
    case kOpJalr: {
      const auto rd = DecodeField1(instruction);
      const auto rs1 = DecodeField2(instruction);
      write_register_bits(rd, next_pc);
      next_pc = AddOffset(read_register_bits(rs1), DecodeImm15(instruction));
      break;
    }
    default:
      // Tensor and system opcodes are intentionally left for future work.
      break;
  }

  pc = next_pc;
  registers[0] = 0;
}

std::uint64_t Emulator::accumulator() const noexcept {
  return accumulator_;
}

std::string_view Emulator::Version() noexcept {
  return "0.1.0";
}

std::array<int64_t, 32> Emulator::GetRegisters() {
  return registers;
}

std::uint32_t Add(std::uint32_t lhs, std::uint32_t rhs) noexcept {
  return lhs + rhs;
}

}  // namespace zenith_emulator
