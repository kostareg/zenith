#pragma once

#include <vector>
#include <string_view>
#include <iostream>

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
    unsigned char* start;
    size_t len;

    std::string ident;

    void display() {
        switch (tt) {
            case TokenType::Colon: std::cout << ":" << std::endl; break;
            case TokenType::Dot: std::cout << "." << std::endl; break;
            case TokenType::Comma: std::cout << "," << std::endl; break;
            case TokenType::NewLine: std::cout << "newline" << std::endl; break;
            case TokenType::Keyword: std::cout << "keyword" << std::endl; break;
            case TokenType::Ident: std::cout << "ident " << ident << std::endl; break;
            case TokenType::Number: std::cout << "number" << std::endl;
        }
    }
};

class Lexer {
  public:
    Lexer() = delete;
    Lexer(std::vector<unsigned char> bytes) : bytes(std::move(bytes)) {}

    std::vector<Token> lex();

  private:
    std::vector<unsigned char> bytes;

    [[nodiscard]] inline Token make_single_char_token(TokenType tt, size_t i) {
        return Token {
            tt,
            &bytes[i],
            1,
            ""
        };
    }
};

}