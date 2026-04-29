#include "zenith_emulator/zenith_emulator.h"

namespace zenith_emulator {

void Emulator::Reset() {
  accumulator_ = 0;
  for (int i = 0; i < 32; ++i) {
    registers[i] = 0;
  }
}

void Emulator::Step(uint32_t instruction) {
  uint8_t op = instruction & 0x7F;
  switch (op) {
    case 0x00: {
      // add rd rs1 rs2
      uint32_t rd  = (instruction >> 7)  & 0x1F;
      uint32_t rs1 = (instruction >> 12) & 0x1F;
      uint32_t rs2 = (instruction >> 17) & 0x1F;

      // todo: bounds check
      registers[rd] = registers[rs1] + registers[rs2];
      break;
    };
    case 0x04: {
      // addi rd rs1 imm
      uint32_t rd  = (instruction >> 7)  & 0x1F;
      uint32_t rs1 = (instruction >> 12) & 0x1F;
      int32_t imm = (int32_t)(instruction << 0) >> 17;

      registers[rd] = registers[rs1] + imm;
      break;
    };
  }
  ++pc;
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
