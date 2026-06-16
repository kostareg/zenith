#pragma once

#include "zenith/lexer.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace zenith::compiler {

class Preprocessor {
  public:
    Preprocessor() = default;

    [[nodiscard]] std::vector<Token> process(std::vector<Token> tokens);

  private:
    std::unordered_map<std::string, std::vector<Token>> macros;
    std::vector<std::string> include_stack;
};

[[nodiscard]] std::vector<Token> preprocess(std::vector<Token> tokens);

} // namespace zenith::compiler
