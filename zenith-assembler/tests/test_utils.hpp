#pragma once

#include "zenith/lexer.hpp"
#include "zenith/parser.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zenith::assembler::test {

inline std::vector<Token> lex_source(std::string source) {
    std::vector<unsigned char> bytes(source.begin(), source.end());
    Lexer lexer(std::move(bytes));
    return lexer.lex();
}

inline Ast parse_source(const std::string& source) {
    Parser parser(lex_source(source));
    return parser.parse();
}

inline std::uint32_t field1(std::uint32_t instruction) {
    return (instruction >> 7) & 0x1F;
}

inline std::uint32_t field2(std::uint32_t instruction) {
    return (instruction >> 12) & 0x1F;
}

inline std::uint32_t field3(std::uint32_t instruction) {
    return (instruction >> 17) & 0x1F;
}

inline std::int32_t imm15(std::uint32_t instruction) {
    return static_cast<std::int32_t>(instruction) >> 17;
}

inline std::int32_t imm20(std::uint32_t instruction) {
    return static_cast<std::int32_t>(instruction) >> 12;
}

} // namespace zenith::assembler::test
