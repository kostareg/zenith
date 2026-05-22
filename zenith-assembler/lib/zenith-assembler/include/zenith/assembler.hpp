#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace zenith::assembler {

class Assembler {
  public:
    Assembler() = default;

    [[nodiscard]] static std::string_view version() noexcept;
    [[nodiscard]] std::string assemble(std::string source) const;
};

} // namespace zenith::assembler
