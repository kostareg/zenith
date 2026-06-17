#pragma once

#include "zenith/compiler.hpp"

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
        , image(ProgramImage{{}, std::move(code), 0}) {}
    Format(std::string name, ProgramImage image)
        : name(std::move(name))
        , image(std::move(image)) {}

    [[nodiscard]] std::vector<std::uint8_t> format() const;

  private:
    std::string name;
    ProgramImage image;
};

} // namespace zenith::assembler
