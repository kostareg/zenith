#pragma once

#include <cstdint>
#include <string_view>
#include <array>

namespace zenith_emulator {

class Emulator {
 public:
  Emulator() = default;

  void Reset();
  void Step(uint32_t);

  [[nodiscard]] std::uint64_t accumulator() const noexcept;
  [[nodiscard]] static std::string_view Version() noexcept;
  [[nodiscard]] std::array<int64_t, 32> GetRegisters();

 private:
  std::uint64_t accumulator_ = 0;
  std::uint64_t pc = 0;
  std::array<int64_t, 32> registers;
};

[[nodiscard]] std::uint32_t Add(uint32_t lhs, uint32_t rhs) noexcept;

}  // namespace zenith_emulator
