#pragma once

#include <string_view>

namespace zenith::compiler {

class Compiler {
  public:
    Compiler() = default;
    [[nodiscard]] static std::string_view version() noexcept;
};

} // namespace zenith::compiler
