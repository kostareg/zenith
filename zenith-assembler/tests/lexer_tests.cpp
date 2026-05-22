#include "test_utils.hpp"

#include "zenith/lexer.hpp"

#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace zenith::assembler;

TEST(Lexer, TokenizesAssemblySyntaxAndSkipsComments) {
    const std::vector<Token> tokens = test::lex_source(".main\n  label: add r0, r1, 42 # ignored\n");

    ASSERT_EQ(tokens.size(), 12);

    EXPECT_EQ(tokens[0].tt, TokenType::Dot);
    EXPECT_EQ(tokens[1].tt, TokenType::Ident);
    EXPECT_EQ(tokens[1].ident, "main");
    EXPECT_EQ(tokens[2].tt, TokenType::NewLine);
    EXPECT_EQ(tokens[3].tt, TokenType::Ident);
    EXPECT_EQ(tokens[3].ident, "label");
    EXPECT_EQ(tokens[4].tt, TokenType::Colon);
    EXPECT_EQ(tokens[5].tt, TokenType::Ident);
    EXPECT_EQ(tokens[5].ident, "add");
    EXPECT_EQ(tokens[6].tt, TokenType::Ident);
    EXPECT_EQ(tokens[6].ident, "r0");
    EXPECT_EQ(tokens[7].tt, TokenType::Comma);
    EXPECT_EQ(tokens[8].tt, TokenType::Ident);
    EXPECT_EQ(tokens[8].ident, "r1");
    EXPECT_EQ(tokens[9].tt, TokenType::Comma);
    EXPECT_EQ(tokens[10].tt, TokenType::Number);
    EXPECT_EQ(tokens[10].num, 42);
    EXPECT_EQ(tokens[11].tt, TokenType::NewLine);
}

} // namespace
