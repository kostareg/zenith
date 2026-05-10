#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

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
  static constexpr std::size_t kMemorySize = 64 * 1024;

  std::uint64_t accumulator_ = 0;
  std::uint64_t pc = 0;
  std::array<int64_t, 32> registers{};
  std::array<std::uint8_t, kMemorySize> memory{};
};

[[nodiscard]] std::uint32_t Add(uint32_t lhs, uint32_t rhs) noexcept;

}  // namespace zenith_emulator
