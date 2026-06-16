#include "zenith/preprocessor.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

std::vector<Token> preprocess_source(std::string source) {
    return preprocess(lex_source(std::move(source)));
}

std::vector<std::string> token_lexemes_without_eof(const std::vector<Token>& tokens) {
    std::vector<std::string> lexemes;
    for (const Token& token : tokens) {
        if (token.type != TokenType::EndOfFile) {
            lexemes.push_back(token.lexeme);
        }
    }
    return lexemes;
}

void set_include_base(const std::filesystem::path& path) {
#ifdef _WIN32
    _putenv_s("ZENITH_COMPILER_INCLUDE_BASE", path.string().c_str());
#else
    setenv("ZENITH_COMPILER_INCLUDE_BASE", path.string().c_str(), 1);
#endif
}

void unset_include_base() {
#ifdef _WIN32
    _putenv_s("ZENITH_COMPILER_INCLUDE_BASE", "");
#else
    unsetenv("ZENITH_COMPILER_INCLUDE_BASE");
#endif
}

TEST(Preprocessor, ExpandsObjectLikeDefinesAndRemovesDirectiveTokens) {
    const std::vector<Token> tokens = preprocess_source("#define ANSWER 42\nint value = ANSWER;");

    EXPECT_EQ(token_lexemes_without_eof(tokens), (std::vector<std::string>{"int", "value", "=", "42", ";"}));
    ASSERT_GE(tokens.size(), 5);
    EXPECT_EQ(tokens[3].type, TokenType::IntegerLiteral);
    EXPECT_EQ(tokens[3].integer_value, 42);
}

TEST(Preprocessor, RecursivelyExpandsDefineReplacements) {
    const std::vector<Token> tokens =
        preprocess_source("#define BASE 40\n#define ANSWER BASE + 2\nint value = ANSWER;");

    EXPECT_EQ(token_lexemes_without_eof(tokens), (std::vector<std::string>{"int", "value", "=", "40", "+", "2", ";"}));
}

TEST(Preprocessor, IncludesLexedFilesFromEnvironmentBase) {
    const std::filesystem::path include_base =
        std::filesystem::temp_directory_path() / "zenith-compiler-preprocessor-test";
    std::filesystem::create_directories(include_base);

    {
        std::ofstream include_file(include_base / "constants.zh");
        include_file << "#define ANSWER 42\nint included = ANSWER;\n";
    }

    set_include_base(include_base);
    const std::vector<Token> tokens = preprocess_source("#include \"constants.zh\"\nint after = ANSWER;");
    unset_include_base();

    EXPECT_EQ(
        token_lexemes_without_eof(tokens),
        (std::vector<std::string>{"int", "included", "=", "42", ";", "int", "after", "=", "42", ";"})
    );

    std::filesystem::remove_all(include_base);
}

TEST(Preprocessor, SupportsMacroExpandedAngleIncludes) {
    const std::filesystem::path include_base =
        std::filesystem::temp_directory_path() / "zenith-compiler-preprocessor-angle-test";
    std::filesystem::create_directories(include_base);

    {
        std::ofstream include_file(include_base / "constants.zh");
        include_file << "int angle = 7;\n";
    }

    set_include_base(include_base);
    const std::vector<Token> tokens = preprocess_source("#define HEADER <constants.zh>\n#include HEADER");
    unset_include_base();

    EXPECT_EQ(token_lexemes_without_eof(tokens), (std::vector<std::string>{"int", "angle", "=", "7", ";"}));

    std::filesystem::remove_all(include_base);
}

TEST(Preprocessor, RequiresIncludeBaseEnvironmentVariable) {
    unset_include_base();

    EXPECT_THROW(preprocess_source("#include \"constants.zh\""), std::runtime_error);
}

TEST(Preprocessor, RejectsFunctionLikeDefines) {
    EXPECT_THROW(preprocess_source("#define DOUBLE(x) x + x\nint value = DOUBLE;"), std::runtime_error);
}

} // namespace
