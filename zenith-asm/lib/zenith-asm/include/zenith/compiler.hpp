#pragma once

#include "zenith/parser.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace zenith::assembler {

class Compiler {
  public:
    Compiler() = delete;
    Compiler(Ast ast)
        : ast(std::move(ast)) {}

    [[nodiscard]] std::vector<std::uint32_t> compile() const;

  private:
    Ast ast;
};

} // namespace zenith::assembler
