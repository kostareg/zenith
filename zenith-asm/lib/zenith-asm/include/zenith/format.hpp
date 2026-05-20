#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zenith::assembler {

class Format {
  public:
    Format() = delete;
    Format(std::string name, std::vector<std::uint32_t> code)
        : name(std::move(name))
        , code(std::move(code)) {}

    [[nodiscard]] std::vector<std::uint8_t> format() const;

  private:
    std::string name;
    std::vector<std::uint32_t> code;
};

} // namespace zenith::assembler
