#include "zenith/parser.hpp"

#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace zenith::assembler {

bool is_section_name(std::string_view name) {
    return name == "main" || name == "data";
}

std::ostream& indent(std::ostream& os, std::size_t depth) {
    for (std::size_t i = 0; i < depth; ++i) {
        os << "  ";
    }

    return os;
}

std::ostream& quoted(std::ostream& os, std::string_view value) {
    os << '"';
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            os << "\\\\";
            break;
        case '"':
            os << "\\\"";
            break;
        case '\n':
            os << "\\n";
            break;
        case '\t':
            os << "\\t";
            break;
        default:
            os << ch;
            break;
        }
    }
    return os << '"';
}

std::ostream& print_operand(std::ostream& os, const Operand& operand, std::size_t depth) {
    indent(os, depth) << "Operand { kind: " << operand.kind;
    switch (operand.kind) {
    case OperandKind::Identifier:
        os << ", ident: ";
        quoted(os, operand.ident);
        break;
    case OperandKind::Number:
        os << ", number: " << operand.number;
        break;
    }
    return os << " }";
}

std::ostream& print_operands_array(std::ostream& os, const std::vector<Operand>& operands, std::size_t depth) {
    if (operands.empty()) return os << "[]";

    os << "[\n";
    for (std::size_t i = 0; i < operands.size(); ++i) {
        print_operand(os, operands[i], depth + 1);
        if (i + 1 < operands.size()) os << ',';
        os << '\n';
    }
    indent(os, depth) << ']';

    return os;
}

std::ostream& print_instruction(std::ostream& os, const Instruction& instruction, std::size_t depth) {
    os << "Instruction {\n";
    indent(os, depth + 1) << "mnemonic: ";
    quoted(os, instruction.mnemonic) << ",\n";
    indent(os, depth + 1) << "operands: ";
    print_operands_array(os, instruction.operands, depth + 1) << '\n';
    indent(os, depth) << '}';

    return os;
}

std::ostream& print_directive(std::ostream& os, const Directive& directive, std::size_t depth) {
    os << "Directive {\n";
    indent(os, depth + 1) << "name: ";
    quoted(os, directive.name) << ",\n";
    indent(os, depth + 1) << "operands: ";
    print_operands_array(os, directive.operands, depth + 1) << '\n';
    indent(os, depth) << '}';

    return os;
}

std::ostream& print_statement_operation(std::ostream& os, const StatementOperation& operation, std::size_t depth) {
    if (const auto* instruction = std::get_if<Instruction>(&operation)) {
        return print_instruction(os, *instruction, depth);
    }

    return print_directive(os, std::get<Directive>(operation), depth);
}

std::ostream& print_statement(std::ostream& os, const Statement& statement, std::size_t depth) {
    os << "Statement {\n";
    indent(os, depth + 1) << "label: ";
    if (statement.label.has_value()) {
        quoted(os, *statement.label);
    } else {
        os << "null";
    }
    os << ",\n";

    indent(os, depth + 1) << "operation: ";
    if (statement.operation.has_value()) {
        print_statement_operation(os, *statement.operation, depth + 1);
    } else {
        os << "null";
    }
    os << '\n';
    indent(os, depth) << '}';

    return os;
}

std::ostream& print_statements_array(std::ostream& os, const std::vector<Statement>& statements, std::size_t depth) {
    if (statements.empty()) return os << "[]";

    os << "[\n";
    for (std::size_t i = 0; i < statements.size(); ++i) {
        indent(os, depth + 1);
        print_statement(os, statements[i], depth + 1);
        if (i + 1 < statements.size()) os << ',';
        os << '\n';
    }
    indent(os, depth) << ']';

    return os;
}

std::ostream& print_section(std::ostream& os, const Section& section, std::size_t depth) {
    os << "Section {\n";
    indent(os, depth + 1) << "name: ";
    quoted(os, section.name) << ",\n";
    indent(os, depth + 1) << "statements: ";
    print_statements_array(os, section.statements, depth + 1) << '\n';
    indent(os, depth) << '}';

    return os;
}

std::ostream& print_sections_array(std::ostream& os, const std::vector<Section>& sections, std::size_t depth) {
    if (sections.empty()) return os << "[]";

    os << "[\n";
    for (std::size_t i = 0; i < sections.size(); ++i) {
        indent(os, depth + 1);
        print_section(os, sections[i], depth + 1);
        if (i + 1 < sections.size()) os << ',';
        os << '\n';
    }
    indent(os, depth) << ']';

    return os;
}

std::ostream& operator<<(std::ostream& os, OperandKind kind) {
    switch (kind) {
    case OperandKind::Identifier:
        return os << "Identifier";
    case OperandKind::Number:
        return os << "Number";
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, const Operand& operand) {
    return print_operand(os, operand, 0);
}

std::ostream& operator<<(std::ostream& os, const Instruction& instruction) {
    return print_instruction(os, instruction, 0);
}

std::ostream& operator<<(std::ostream& os, const Directive& directive) {
    return print_directive(os, directive, 0);
}

std::ostream& operator<<(std::ostream& os, const StatementOperation& operation) {
    return print_statement_operation(os, operation, 0);
}

std::ostream& operator<<(std::ostream& os, const Statement& statement) {
    return print_statement(os, statement, 0);
}

std::ostream& operator<<(std::ostream& os, const Section& section) {
    return print_section(os, section, 0);
}

std::ostream& operator<<(std::ostream& os, const Ast& ast) {
    os << "Ast {\n";
    indent(os, 1) << "sections: ";
    print_sections_array(os, ast.sections, 1) << '\n';
    return os << '}';
}

Ast Parser::parse() {
    Ast ast;

    skip_new_lines();
    while (!is_at_end()) {
        if (!is_section_start()) {
            throw std::runtime_error("expected a section directive such as .main or .data");
        }

        ast.sections.push_back(parse_section());
        skip_new_lines();
    }

    return ast;
}

bool Parser::is_at_end() const {
    return current >= tokens.size();
}

bool Parser::check(TokenType tt, std::size_t offset) const {
    const std::size_t index = current + offset;
    return index < tokens.size() && tokens[index].tt == tt;
}

bool Parser::is_line_end(std::size_t offset) const {
    const std::size_t index = current + offset;
    return index >= tokens.size() || tokens[index].tt == TokenType::NewLine;
}

bool Parser::is_section_start() const {
    return check(TokenType::Dot) && check(TokenType::Ident, 1) && is_section_name(tokens[current + 1].ident) &&
           is_line_end(2);
}

const Token& Parser::advance() {
    if (is_at_end()) throw std::runtime_error("unexpected end of input");

    const Token& token = tokens[current++];
    if (token.tt == TokenType::NewLine) ++line;
    return token;
}

const Token& Parser::consume(TokenType tt, std::string_view message) {
    if (check(tt)) return advance();
    throw std::runtime_error(static_cast<std::string>(message));
}

bool Parser::match(TokenType tt) {
    if (!check(tt)) return false;

    advance();
    return true;
}

void Parser::skip_new_lines() {
    while (match(TokenType::NewLine)) {
    }
}

void Parser::consume_line_end(std::string_view context) {
    if (is_at_end()) return;
    if (!match(TokenType::NewLine)) throw std::runtime_error(static_cast<std::string>(context));

    skip_new_lines();
}

Section Parser::parse_section() {
    consume(TokenType::Dot, "expected '.' before section name");
    const Token& name = consume(TokenType::Ident, "expected section name");

    Section section{
        name.ident,
        {},
    };

    consume_line_end("expected end of line after section name");

    while (!is_at_end() && !is_section_start()) {
        if (match(TokenType::NewLine)) continue;
        section.statements.push_back(parse_statement());
    }

    return section;
}

Statement Parser::parse_statement() {
    Statement statement;

    if (check(TokenType::Ident) && check(TokenType::Colon, 1)) {
        statement.label = advance().ident;
        advance();
    }

    if (!is_line_end()) {
        statement.operation = parse_operation();
    } else if (!statement.label.has_value()) {
        throw std::runtime_error("expected a label, instruction, or directive");
    }

    consume_line_end("expected end of line after statement");
    return statement;
}

StatementOperation Parser::parse_operation() {
    if (check(TokenType::Dot)) return parse_directive();
    if (check(TokenType::Ident)) return parse_instruction();

    throw std::runtime_error("expected instruction or directive");
}

Instruction Parser::parse_instruction() {
    const Token& mnemonic = consume(TokenType::Ident, "expected instruction mnemonic");

    return Instruction{
        mnemonic.ident,
        parse_operands(),
    };
}

Directive Parser::parse_directive() {
    consume(TokenType::Dot, "expected '.' before directive name");
    const Token& name = consume(TokenType::Ident, "expected directive name");

    return Directive{
        name.ident,
        parse_operands(),
    };
}

std::vector<Operand> Parser::parse_operands() {
    std::vector<Operand> operands;
    if (is_line_end()) return operands;

    operands.push_back(parse_operand());
    while (!is_line_end()) {
        consume(TokenType::Comma, "expected ',' between operands");
        if (is_line_end()) throw std::runtime_error("expected operand after ','");
        operands.push_back(parse_operand());
    }

    return operands;
}

Operand Parser::parse_operand() {
    if (check(TokenType::Ident)) {
        return Operand{
            OperandKind::Identifier,
            advance().ident,
            0,
        };
    }

    if (check(TokenType::Number)) {
        return Operand{
            OperandKind::Number,
            "",
            advance().num,
        };
    }

    throw std::runtime_error("expected operand");
}

} // namespace zenith::assembler
