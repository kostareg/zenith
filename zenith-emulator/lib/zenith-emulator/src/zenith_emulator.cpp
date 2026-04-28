#include "zenith_emulator/zenith_emulator.h"

namespace zenith_emulator {

void Emulator::Reset() {
  accumulator_ = 0;
}

void Emulator::Step() {
  ++accumulator_;
}

std::uint64_t Emulator::accumulator() const noexcept {
  return accumulator_;
}

std::string_view Emulator::Version() noexcept {
  return "0.1.0";
}

std::uint32_t Add(std::uint32_t lhs, std::uint32_t rhs) noexcept {
  return lhs + rhs;
}

}  // namespace zenith_emulator
