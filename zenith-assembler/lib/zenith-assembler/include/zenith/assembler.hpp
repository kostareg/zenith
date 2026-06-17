#pragma once

#include <cstdint>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace zenith::assembler {

struct AssembledProgram {
    std::string machine_code;
    std::vector<std::uint8_t> data;
};

class Assembler {
  public:
    Assembler() = default;

    [[nodiscard]] static std::string_view version() noexcept;
    [[nodiscard]] std::string assemble(std::string source) const;
    [[nodiscard]] AssembledProgram assemble_program(std::string source) const;
};

} // namespace zenith::assembler
