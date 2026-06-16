#include "zenith/preprocessor.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace zenith::compiler {
namespace {

constexpr std::string_view include_base_env = "ZENITH_COMPILER_INCLUDE_BASE";

[[nodiscard]] bool is_eof(const Token& token) noexcept {
    return token.type == TokenType::EndOfFile;
}

[[nodiscard]] std::size_t line_end(const std::vector<Token>& tokens, std::size_t start) noexcept {
    const std::size_t line = tokens[start].line;
    std::size_t end = start;

    while (end < tokens.size() && !is_eof(tokens[end]) && tokens[end].line == line) {
        ++end;
    }

    return end;
}

[[nodiscard]] std::string location(const Token& token) {
    return "line " + std::to_string(token.line) + ", column " + std::to_string(token.column);
}

[[noreturn]] void fail_at(const Token& token, std::string message) {
    throw std::runtime_error(location(token) + ": " + std::move(message));
}

[[nodiscard]] bool is_directive_name(const Token& token, std::string_view name) noexcept {
    return token.type == TokenType::Identifier && token.lexeme == name;
}

[[nodiscard]] bool starts_function_like_macro(const Token& name, const Token& next) noexcept {
    return next.type == TokenType::LeftParen && next.line == name.line &&
           next.column == name.column + name.lexeme.size();
}

void append_expanded_token(
    const Token& token,
    const std::unordered_map<std::string, std::vector<Token>>& macros,
    std::unordered_set<std::string>& active_macros,
    std::vector<Token>& output
) {
    if (token.type != TokenType::Identifier) {
        output.push_back(token);
        return;
    }

    const auto macro = macros.find(token.lexeme);
    if (macro == macros.end() || active_macros.contains(token.lexeme)) {
        output.push_back(token);
        return;
    }

    active_macros.insert(token.lexeme);
    for (const Token& replacement : macro->second) {
        append_expanded_token(replacement, macros, active_macros, output);
    }
    active_macros.erase(token.lexeme);
}

[[nodiscard]] std::filesystem::path include_path(std::string include_name, const Token& token) {
    std::filesystem::path relative(std::move(include_name));
    if (relative.empty()) {
        fail_at(token, "#include path cannot be empty");
    }
    if (relative.is_absolute()) {
        fail_at(token, "#include path must be relative to ZENITH_COMPILER_INCLUDE_BASE");
    }

    const char* base = std::getenv(include_base_env.data());
    if (base == nullptr || *base == '\0') {
        fail_at(token, "ZENITH_COMPILER_INCLUDE_BASE is not set");
    }

    return std::filesystem::path(base) / relative;
}

[[nodiscard]] std::filesystem::path include_path_from_tokens(const std::vector<Token>& tokens, const Token& directive) {
    if (tokens.empty()) {
        fail_at(directive, "#include expects a path");
    }

    if (tokens.size() == 1 && tokens.front().type == TokenType::StringLiteral) {
        return include_path(tokens.front().text, tokens.front());
    }

    if (tokens.size() >= 3 && tokens.front().type == TokenType::Less && tokens.back().type == TokenType::Greater) {
        std::string include_name;
        for (std::size_t i = 1; i + 1 < tokens.size(); ++i) {
            include_name += tokens[i].lexeme;
        }
        return include_path(std::move(include_name), tokens.front());
    }

    fail_at(tokens.front(), "#include expects a string literal or angle-bracket path");
}

[[nodiscard]] std::string path_key(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().string();
}

[[nodiscard]] std::string read_text_file(const std::filesystem::path& path, const Token& include_token) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        fail_at(include_token, "failed to open include file: " + path.string());
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

class IncludeStackEntry {
  public:
    IncludeStackEntry(std::vector<std::string>& include_stack, std::string path)
        : include_stack(include_stack) {
        include_stack.push_back(std::move(path));
    }

    ~IncludeStackEntry() {
        include_stack.pop_back();
    }

    IncludeStackEntry(const IncludeStackEntry&) = delete;
    IncludeStackEntry& operator=(const IncludeStackEntry&) = delete;

  private:
    std::vector<std::string>& include_stack;
};

} // namespace

std::vector<Token> Preprocessor::process(std::vector<Token> tokens) {
    std::vector<Token> output;
    output.reserve(tokens.size());
    std::unordered_set<std::string> active_macros;

    for (std::size_t i = 0; i < tokens.size();) {
        const Token& token = tokens[i];

        if (is_eof(token)) {
            ++i;
            continue;
        }

        if (token.type != TokenType::Hash) {
            append_expanded_token(token, macros, active_macros, output);
            ++i;
            continue;
        }

        const std::size_t end = line_end(tokens, i);
        if (i + 1 >= end) {
            fail_at(token, "expected preprocessor directive after '#'");
        }

        const Token& directive = tokens[i + 1];
        if (is_directive_name(directive, "define")) {
            if (i + 2 >= end) {
                fail_at(directive, "#define expects a macro name");
            }

            const Token& name = tokens[i + 2];
            if (name.type != TokenType::Identifier) {
                fail_at(name, "#define macro name must be an identifier");
            }
            if (i + 3 < end && starts_function_like_macro(name, tokens[i + 3])) {
                fail_at(name, "function-like #define macros are not supported");
            }

            macros[name.lexeme] = std::vector<Token>(
                tokens.begin() + static_cast<std::ptrdiff_t>(i + 3), tokens.begin() + static_cast<std::ptrdiff_t>(end)
            );
            i = end;
            continue;
        }

        if (is_directive_name(directive, "include")) {
            if (i + 2 >= end) {
                fail_at(directive, "#include expects a path");
            }

            std::vector<Token> include_tokens;
            for (std::size_t j = i + 2; j < end; ++j) {
                append_expanded_token(tokens[j], macros, active_macros, include_tokens);
            }

            const std::filesystem::path resolved_path = include_path_from_tokens(include_tokens, directive);
            const std::string resolved_key = path_key(resolved_path);
            if (std::find(include_stack.begin(), include_stack.end(), resolved_key) != include_stack.end()) {
                fail_at(include_tokens.front(), "recursive #include detected: " + resolved_path.string());
            }

            const IncludeStackEntry include_entry(include_stack, resolved_key);
            std::vector<Token> included_tokens = Lexer(read_text_file(resolved_path, include_tokens.front())).lex();
            std::vector<Token> included_output = process(std::move(included_tokens));

            for (const Token& included_token : included_output) {
                if (!is_eof(included_token)) {
                    output.push_back(included_token);
                }
            }

            i = end;
            continue;
        }

        fail_at(directive, "unknown preprocessor directive: " + directive.lexeme);
    }

    output.push_back(
        Token{
            TokenType::EndOfFile,
            "",
            tokens.empty() ? 1 : tokens.back().line,
            tokens.empty() ? 1 : tokens.back().column,
            0,
            ""
        }
    );
    return output;
}

std::vector<Token> preprocess(std::vector<Token> tokens) {
    return Preprocessor().process(std::move(tokens));
}

} // namespace zenith::compiler
