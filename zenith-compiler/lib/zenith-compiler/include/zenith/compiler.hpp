#pragma once

#include "zenith/parser.hpp"

#include <string>
#include <string_view>

namespace zenith::compiler {

class Compiler {
  public:
    Compiler() = default;
    [[nodiscard]] static std::string_view version() noexcept;
    [[nodiscard]] std::string compile(const Ast& ast) const;
    [[nodiscard]] std::string compile(std::string source) const;
};

} // namespace zenith::compiler
