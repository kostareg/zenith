#include "zenith/lexer.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace zenith::compiler;

std::vector<Token> lex_source(std::string source) {
    return Lexer(std::move(source)).lex();
}

std::vector<TokenType> token_types(const std::vector<Token>& tokens) {
    std::vector<TokenType> types;
    types.reserve(tokens.size());

    for (const Token& token : tokens) {
        types.push_back(token.type);
    }

    return types;
}

TEST(Lexer, TokenizesKeywordsAndBuiltinTypes) {
    const std::vector<Token> tokens = lex_source(
        "if else while for break continue return switch case default "
        "struct enum static true false bool void char short int long signed unsigned "
        "int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t");

    EXPECT_EQ(token_types(tokens), (std::vector<TokenType>{
                                      TokenType::KeywordIf,
                                      TokenType::KeywordElse,
                                      TokenType::KeywordWhile,
                                      TokenType::KeywordFor,
                                      TokenType::KeywordBreak,
                                      TokenType::KeywordContinue,
                                      TokenType::KeywordReturn,
                                      TokenType::KeywordSwitch,
                                      TokenType::KeywordCase,
                                      TokenType::KeywordDefault,
                                      TokenType::KeywordStruct,
                                      TokenType::KeywordEnum,
                                      TokenType::KeywordStatic,
                                      TokenType::KeywordTrue,
                                      TokenType::KeywordFalse,
                                      TokenType::KeywordBool,
                                      TokenType::KeywordVoid,
                                      TokenType::KeywordChar,
                                      TokenType::KeywordShort,
                                      TokenType::KeywordInt,
                                      TokenType::KeywordLong,
                                      TokenType::KeywordSigned,
                                      TokenType::KeywordUnsigned,
                                      TokenType::KeywordInt8,
                                      TokenType::KeywordInt16,
                                      TokenType::KeywordInt32,
                                      TokenType::KeywordInt64,
                                      TokenType::KeywordUInt8,
                                      TokenType::KeywordUInt16,
                                      TokenType::KeywordUInt32,
                                      TokenType::KeywordUInt64,
                                      TokenType::EndOfFile,
                                  }));
}

TEST(Lexer, LeavesUnsupportedCWordsAsIdentifiers) {
    const std::vector<Token> tokens = lex_source("sizeof const typedef union extern");

    ASSERT_EQ(tokens.size(), 6);
    for (std::size_t i = 0; i < tokens.size() - 1; ++i) {
        EXPECT_EQ(tokens[i].type, TokenType::Identifier);
    }
    EXPECT_EQ(tokens.back().type, TokenType::EndOfFile);
}

TEST(Lexer, TokenizesOperatorsAndPunctuation) {
    const std::vector<std::pair<std::string, TokenType>> cases = {
        {"(", TokenType::LeftParen},
        {")", TokenType::RightParen},
        {"{", TokenType::LeftBrace},
        {"}", TokenType::RightBrace},
        {"[", TokenType::LeftBracket},
        {"]", TokenType::RightBracket},
        {";", TokenType::Semicolon},
        {",", TokenType::Comma},
        {".", TokenType::Dot},
        {":", TokenType::Colon},
        {"?", TokenType::Question},
        {"+", TokenType::Plus},
        {"-", TokenType::Minus},
        {"*", TokenType::Star},
        {"/", TokenType::Slash},
        {"%", TokenType::Percent},
        {"++", TokenType::PlusPlus},
        {"--", TokenType::MinusMinus},
        {"=", TokenType::Equal},
        {"+=", TokenType::PlusEqual},
        {"-=", TokenType::MinusEqual},
        {"*=", TokenType::StarEqual},
        {"/=", TokenType::SlashEqual},
        {"%=", TokenType::PercentEqual},
        {"==", TokenType::EqualEqual},
        {"!=", TokenType::BangEqual},
        {"<", TokenType::Less},
        {"<=", TokenType::LessEqual},
        {">", TokenType::Greater},
        {">=", TokenType::GreaterEqual},
        {"!", TokenType::Bang},
        {"~", TokenType::Tilde},
        {"&", TokenType::Ampersand},
        {"|", TokenType::Pipe},
        {"^", TokenType::Caret},
        {"&&", TokenType::AmpersandAmpersand},
        {"||", TokenType::PipePipe},
        {"<<", TokenType::LessLess},
        {">>", TokenType::GreaterGreater},
        {"&=", TokenType::AmpersandEqual},
        {"|=", TokenType::PipeEqual},
        {"^=", TokenType::CaretEqual},
        {"<<=", TokenType::LessLessEqual},
        {">>=", TokenType::GreaterGreaterEqual},
        {"->", TokenType::Arrow},
    };

    for (const auto& [source, type] : cases) {
        const std::vector<Token> tokens = lex_source(source);
        ASSERT_EQ(tokens.size(), 2) << source;
        EXPECT_EQ(tokens[0].type, type) << source;
        EXPECT_EQ(tokens[1].type, TokenType::EndOfFile) << source;
    }
}

TEST(Lexer, TokenizesCStyleExpressionWithTernary) {
    const std::vector<Token> tokens = lex_source("result = ptr->field ? values[0] + 1 : 0;");

    EXPECT_EQ(token_types(tokens), (std::vector<TokenType>{
                                      TokenType::Identifier,
                                      TokenType::Equal,
                                      TokenType::Identifier,
                                      TokenType::Arrow,
                                      TokenType::Identifier,
                                      TokenType::Question,
                                      TokenType::Identifier,
                                      TokenType::LeftBracket,
                                      TokenType::IntegerLiteral,
                                      TokenType::RightBracket,
                                      TokenType::Plus,
                                      TokenType::IntegerLiteral,
                                      TokenType::Colon,
                                      TokenType::IntegerLiteral,
                                      TokenType::Semicolon,
                                      TokenType::EndOfFile,
                                  }));
}

TEST(Lexer, TokenizesIntegerCharacterAndStringLiterals) {
    const std::vector<Token> tokens = lex_source(R"('\n' "hi\t\"there" 42 077 0xffULL 10u 11ll 12ul)");

    ASSERT_EQ(tokens.size(), 9);

    EXPECT_EQ(tokens[0].type, TokenType::CharacterLiteral);
    EXPECT_EQ(tokens[0].integer_value, static_cast<std::uint64_t>('\n'));
    EXPECT_EQ(tokens[0].text, "\n");

    EXPECT_EQ(tokens[1].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[1].text, "hi\t\"there");

    EXPECT_EQ(tokens[2].integer_value, 42);
    EXPECT_EQ(tokens[3].integer_value, 63);
    EXPECT_EQ(tokens[4].integer_value, 255);
    EXPECT_EQ(tokens[5].integer_value, 10);
    EXPECT_EQ(tokens[6].integer_value, 11);
    EXPECT_EQ(tokens[7].integer_value, 12);
    EXPECT_EQ(tokens[8].type, TokenType::EndOfFile);
}

TEST(Lexer, SkipsWhitespaceAndComments) {
    const std::vector<Token> tokens = lex_source("int // ignored\n/* also ignored */ value");

    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, TokenType::KeywordInt);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "value");
    EXPECT_EQ(tokens[2].type, TokenType::EndOfFile);
}

TEST(Lexer, ReportsUnsupportedLexicalForms) {
    EXPECT_THROW(lex_source("#define X 1"), std::runtime_error);
    EXPECT_THROW(lex_source("1.5"), std::runtime_error);
    EXPECT_THROW(lex_source("0b1010"), std::runtime_error);
}

} // namespace
