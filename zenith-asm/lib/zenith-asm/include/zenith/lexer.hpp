#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace zenith::assembler {

enum TokenType {
    Colon,
    Dot,
    Comma,
    NewLine,
    Keyword,
    Ident,
    Number,
};

struct Token {
    TokenType tt;
    const unsigned char* start;
    size_t len;

    std::string ident;
    int num;

    void display() {
        switch (tt) {
        case TokenType::Colon:
            std::cout << ":" << std::endl;
            break;
        case TokenType::Dot:
            std::cout << "." << std::endl;
            break;
        case TokenType::Comma:
            std::cout << "," << std::endl;
            break;
        case TokenType::NewLine:
            std::cout << "newline" << std::endl;
            break;
        case TokenType::Keyword:
            std::cout << "keyword" << std::endl;
            break;
        case TokenType::Ident:
            std::cout << "ident " << ident << std::endl;
            break;
        case TokenType::Number:
            std::cout << "number " << num << std::endl;
        }
    }
};

class Lexer {
  public:
    Lexer() = delete;
    Lexer(std::vector<unsigned char> bytes)
        : bytes(std::move(bytes)) {}

    std::vector<Token> lex() const;

  private:
    std::vector<unsigned char> bytes;

    [[nodiscard]] inline Token make_single_char_token(TokenType tt, size_t i) const {
        return Token{tt, &bytes[i], 1, "", 0};
    }
};

} // namespace zenith::assembler
