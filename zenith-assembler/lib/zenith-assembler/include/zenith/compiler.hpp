#pragma once

#include "zenith/parser.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace zenith::assembler {

struct ProgramImage {
    std::vector<std::uint8_t> data;
    std::vector<std::uint32_t> code;
    std::uint64_t entry_offset = 0;
};

class Compiler {
  public:
    Compiler() = delete;
    Compiler(Ast ast)
        : ast(std::move(ast)) {}

    [[nodiscard]] std::vector<std::uint32_t> compile() const;
    [[nodiscard]] ProgramImage compile_program() const;

  private:
    Ast ast;
};

} // namespace zenith::assembler
