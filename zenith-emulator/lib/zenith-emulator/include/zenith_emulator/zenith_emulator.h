#pragma once

#include <cstdint>
#include <string_view>

namespace zenith_emulator {

class Emulator {
 public:
  Emulator() = default;

  void Reset();
  void Step();

  [[nodiscard]] std::uint64_t accumulator() const noexcept;
  [[nodiscard]] static std::string_view Version() noexcept;

 private:
  std::uint64_t accumulator_ = 0;
};

[[nodiscard]] std::uint32_t Add(std::uint32_t lhs, std::uint32_t rhs) noexcept;

}  // namespace zenith_emulator
