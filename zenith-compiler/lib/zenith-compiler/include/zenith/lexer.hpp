#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace zenith::compiler {

enum class TokenType {
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Semicolon,
    Comma,
    Dot,
    Colon,
    Question,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    PlusPlus,
    MinusMinus,

    Equal,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,

    EqualEqual,
    BangEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,

    Bang,
    Tilde,
    Ampersand,
    Pipe,
    Caret,
    AmpersandAmpersand,
    PipePipe,
    LessLess,
    GreaterGreater,
    AmpersandEqual,
    PipeEqual,
    CaretEqual,
    LessLessEqual,
    GreaterGreaterEqual,
    Arrow,

    Identifier,
    IntegerLiteral,
    CharacterLiteral,
    StringLiteral,

    KeywordIf,
    KeywordElse,
    KeywordWhile,
    KeywordFor,
    KeywordBreak,
    KeywordContinue,
    KeywordReturn,
    KeywordSwitch,
    KeywordCase,
    KeywordDefault,
    KeywordStruct,
    KeywordEnum,
    KeywordStatic,
    KeywordTrue,
    KeywordFalse,
    KeywordBool,
    KeywordVoid,
    KeywordChar,
    KeywordShort,
    KeywordInt,
    KeywordLong,
    KeywordSigned,
    KeywordUnsigned,
    KeywordInt8,
    KeywordInt16,
    KeywordInt32,
    KeywordInt64,
    KeywordUInt8,
    KeywordUInt16,
    KeywordUInt32,
    KeywordUInt64,

    EndOfFile,
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t column;

    std::uint64_t integer_value = 0;
    std::string text;
};

class Lexer {
  public:
    Lexer() = delete;
    explicit Lexer(std::string source);
    explicit Lexer(std::vector<unsigned char> bytes);

    [[nodiscard]] std::vector<Token> lex() const;

  private:
    std::string source;
};

[[nodiscard]] std::string_view token_type_name(TokenType type) noexcept;

} // namespace zenith::compiler
