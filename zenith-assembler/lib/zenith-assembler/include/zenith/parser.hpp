#pragma once

#include "zenith/lexer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zenith::assembler {

enum class OperandKind {
    Identifier,
    Number,
};

struct Operand {
    OperandKind kind;
    std::string ident;
    std::int64_t number = 0;
};

struct Instruction {
    std::string mnemonic;
    std::vector<Operand> operands;
};

struct Directive {
    std::string name;
    std::vector<Operand> operands;
};

using StatementOperation = std::variant<Instruction, Directive>;

struct Statement {
    std::optional<std::string> label;
    std::optional<StatementOperation> operation;
};

struct Section {
    std::string name;
    std::vector<Statement> statements;
};

struct Ast {
    std::vector<Section> sections;
};

std::ostream& operator<<(std::ostream& os, OperandKind kind);
std::ostream& operator<<(std::ostream& os, const Operand& operand);
std::ostream& operator<<(std::ostream& os, const Instruction& instruction);
std::ostream& operator<<(std::ostream& os, const Directive& directive);
std::ostream& operator<<(std::ostream& os, const StatementOperation& operation);
std::ostream& operator<<(std::ostream& os, const Statement& statement);
std::ostream& operator<<(std::ostream& os, const Section& section);
std::ostream& operator<<(std::ostream& os, const Ast& ast);

class Parser {
  public:
    Parser() = delete;
    Parser(std::vector<Token> tokens)
        : tokens(std::move(tokens)) {}

    Ast parse();

  private:
    std::vector<Token> tokens;
    std::size_t current = 0;
    std::size_t line = 1;

    [[nodiscard]] bool is_at_end() const;
    [[nodiscard]] bool check(TokenType tt, std::size_t offset = 0) const;
    [[nodiscard]] bool is_line_end(std::size_t offset = 0) const;
    [[nodiscard]] bool is_section_start() const;

    const Token& advance();
    const Token& consume(TokenType tt, std::string_view message);
    bool match(TokenType tt);
    void skip_new_lines();
    void consume_line_end(std::string_view context);

    Section parse_section();
    Statement parse_statement();
    StatementOperation parse_operation();
    Instruction parse_instruction();
    Directive parse_directive();
    std::vector<Operand> parse_operands();
    Operand parse_operand();

    [[noreturn]] void fail(std::string_view message) const;
};

} // namespace zenith::assembler
