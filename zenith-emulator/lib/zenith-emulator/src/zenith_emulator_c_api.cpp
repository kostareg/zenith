#include "zenith_emulator/zenith_emulator_c_api.h"

#include "zenith_emulator/zenith_emulator.h"

namespace {

zenith_emulator::Emulator& GlobalEmulator() {
  static zenith_emulator::Emulator emulator;
  return emulator;
}

std::uint64_t PackVersion() {
  constexpr std::uint64_t major = 0;
  constexpr std::uint64_t minor = 1;
  constexpr std::uint64_t patch = 0;
  return (major << 32U) | (minor << 16U) | patch;
}

}  // namespace

extern "C" uint32_t zenith_emulator_add(uint32_t lhs, uint32_t rhs) {
  return zenith_emulator::Add(lhs, rhs);
}

extern "C" void zenith_emulator_reset(void) {
  GlobalEmulator().Reset();
}

extern "C" void zenith_emulator_step(uint32_t i) {
  GlobalEmulator().Step(i);
}

extern "C" uint64_t zenith_emulator_version(void) {
  return PackVersion();
}
