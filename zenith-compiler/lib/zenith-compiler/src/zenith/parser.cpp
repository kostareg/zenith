#include "zenith/parser.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace zenith::compiler {
namespace {

[[nodiscard]] SourceLocation location_from(const Token& token) noexcept {
    return SourceLocation{token.line, token.column};
}

[[nodiscard]] Token make_eof_token() {
    return Token{TokenType::EndOfFile, "", 1, 1, 0, ""};
}

[[nodiscard]] bool is_primitive_type_token(TokenType type) noexcept {
    switch (type) {
    case TokenType::KeywordBool:
    case TokenType::KeywordVoid:
    case TokenType::KeywordChar:
    case TokenType::KeywordShort:
    case TokenType::KeywordInt:
    case TokenType::KeywordLong:
    case TokenType::KeywordSigned:
    case TokenType::KeywordUnsigned:
    case TokenType::KeywordInt8:
    case TokenType::KeywordInt16:
    case TokenType::KeywordInt32:
    case TokenType::KeywordInt64:
    case TokenType::KeywordUInt8:
    case TokenType::KeywordUInt16:
    case TokenType::KeywordUInt32:
    case TokenType::KeywordUInt64:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] PrimitiveType primitive_type_from(TokenType type) {
    switch (type) {
    case TokenType::KeywordBool:
        return PrimitiveType::Bool;
    case TokenType::KeywordVoid:
        return PrimitiveType::Void;
    case TokenType::KeywordChar:
        return PrimitiveType::Char;
    case TokenType::KeywordShort:
        return PrimitiveType::Short;
    case TokenType::KeywordInt:
        return PrimitiveType::Int;
    case TokenType::KeywordLong:
        return PrimitiveType::Long;
    case TokenType::KeywordSigned:
        return PrimitiveType::Signed;
    case TokenType::KeywordUnsigned:
        return PrimitiveType::Unsigned;
    case TokenType::KeywordInt8:
        return PrimitiveType::Int8;
    case TokenType::KeywordInt16:
        return PrimitiveType::Int16;
    case TokenType::KeywordInt32:
        return PrimitiveType::Int32;
    case TokenType::KeywordInt64:
        return PrimitiveType::Int64;
    case TokenType::KeywordUInt8:
        return PrimitiveType::UInt8;
    case TokenType::KeywordUInt16:
        return PrimitiveType::UInt16;
    case TokenType::KeywordUInt32:
        return PrimitiveType::UInt32;
    case TokenType::KeywordUInt64:
        return PrimitiveType::UInt64;
    default:
        throw std::runtime_error("internal parser error: token is not a primitive type");
    }
}

[[nodiscard]] bool is_assignment_token(TokenType type) noexcept {
    switch (type) {
    case TokenType::Equal:
    case TokenType::PlusEqual:
    case TokenType::MinusEqual:
    case TokenType::StarEqual:
    case TokenType::SlashEqual:
    case TokenType::PercentEqual:
    case TokenType::LessLessEqual:
    case TokenType::GreaterGreaterEqual:
    case TokenType::AmpersandEqual:
    case TokenType::CaretEqual:
    case TokenType::PipeEqual:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] AssignmentOperator assignment_operator_from(TokenType type) {
    switch (type) {
    case TokenType::Equal:
        return AssignmentOperator::Assign;
    case TokenType::PlusEqual:
        return AssignmentOperator::AddAssign;
    case TokenType::MinusEqual:
        return AssignmentOperator::SubtractAssign;
    case TokenType::StarEqual:
        return AssignmentOperator::MultiplyAssign;
    case TokenType::SlashEqual:
        return AssignmentOperator::DivideAssign;
    case TokenType::PercentEqual:
        return AssignmentOperator::ModuloAssign;
    case TokenType::LessLessEqual:
        return AssignmentOperator::ShiftLeftAssign;
    case TokenType::GreaterGreaterEqual:
        return AssignmentOperator::ShiftRightAssign;
    case TokenType::AmpersandEqual:
        return AssignmentOperator::BitwiseAndAssign;
    case TokenType::CaretEqual:
        return AssignmentOperator::BitwiseXorAssign;
    case TokenType::PipeEqual:
        return AssignmentOperator::BitwiseOrAssign;
    default:
        throw std::runtime_error("internal parser error: token is not an assignment operator");
    }
}

[[nodiscard]] int binary_precedence(TokenType type) noexcept {
    switch (type) {
    case TokenType::PipePipe:
        return 1;
    case TokenType::AmpersandAmpersand:
        return 2;
    case TokenType::Pipe:
        return 3;
    case TokenType::Caret:
        return 4;
    case TokenType::Ampersand:
        return 5;
    case TokenType::EqualEqual:
    case TokenType::BangEqual:
        return 6;
    case TokenType::Less:
    case TokenType::LessEqual:
    case TokenType::Greater:
    case TokenType::GreaterEqual:
        return 7;
    case TokenType::LessLess:
    case TokenType::GreaterGreater:
        return 8;
    case TokenType::Plus:
    case TokenType::Minus:
        return 9;
    case TokenType::Star:
    case TokenType::Slash:
    case TokenType::Percent:
        return 10;
    default:
        return -1;
    }
}

[[nodiscard]] BinaryOperator binary_operator_from(TokenType type) {
    switch (type) {
    case TokenType::Star:
        return BinaryOperator::Multiply;
    case TokenType::Slash:
        return BinaryOperator::Divide;
    case TokenType::Percent:
        return BinaryOperator::Modulo;
    case TokenType::Plus:
        return BinaryOperator::Add;
    case TokenType::Minus:
        return BinaryOperator::Subtract;
    case TokenType::LessLess:
        return BinaryOperator::ShiftLeft;
    case TokenType::GreaterGreater:
        return BinaryOperator::ShiftRight;
    case TokenType::Less:
        return BinaryOperator::Less;
    case TokenType::LessEqual:
        return BinaryOperator::LessEqual;
    case TokenType::Greater:
        return BinaryOperator::Greater;
    case TokenType::GreaterEqual:
        return BinaryOperator::GreaterEqual;
    case TokenType::EqualEqual:
        return BinaryOperator::Equal;
    case TokenType::BangEqual:
        return BinaryOperator::NotEqual;
    case TokenType::Ampersand:
        return BinaryOperator::BitwiseAnd;
    case TokenType::Caret:
        return BinaryOperator::BitwiseXor;
    case TokenType::Pipe:
        return BinaryOperator::BitwiseOr;
    case TokenType::AmpersandAmpersand:
        return BinaryOperator::LogicalAnd;
    case TokenType::PipePipe:
        return BinaryOperator::LogicalOr;
    default:
        throw std::runtime_error("internal parser error: token is not a binary operator");
    }
}

template <typename Node> [[nodiscard]] ExpressionPtr make_expression(SourceLocation location, Node node) {
    return std::make_unique<Expression>(Expression{location, Expression::Node{std::move(node)}});
}

template <typename Node> [[nodiscard]] StatementPtr make_statement(SourceLocation location, Node node) {
    return std::make_unique<Statement>(Statement{location, Statement::Node{std::move(node)}});
}

[[nodiscard]] TypePtr make_type(Type type) {
    return std::make_unique<Type>(std::move(type));
}

[[nodiscard]] std::string token_description(const Token& token) {
    if (token.type == TokenType::EndOfFile) {
        return "end of input";
    }
    if (!token.lexeme.empty()) {
        return "'" + token.lexeme + "'";
    }
    return std::string(token_type_name(token.type));
}

} // namespace

Parser::Parser(std::vector<Token> tokens)
    : tokens(std::move(tokens)) {
    if (this->tokens.empty()) {
        this->tokens.push_back(make_eof_token());
    } else if (this->tokens.back().type != TokenType::EndOfFile) {
        Token eof = make_eof_token();
        eof.line = this->tokens.back().line;
        eof.column = this->tokens.back().column + this->tokens.back().lexeme.size();
        this->tokens.push_back(std::move(eof));
    }
}

Ast Parser::parse() {
    Ast ast;

    while (!is_at_end()) {
        ast.declarations.push_back(parse_external_declaration());
    }

    consume(TokenType::EndOfFile, "expected end of input");
    return ast;
}

bool Parser::is_at_end() const noexcept {
    return peek().type == TokenType::EndOfFile;
}

bool Parser::check(TokenType type, std::size_t offset) const noexcept {
    return peek(offset).type == type;
}

bool Parser::is_type_start(std::size_t offset) const noexcept {
    const TokenType type = peek(offset).type;
    return is_primitive_type_token(type) || type == TokenType::KeywordStruct || type == TokenType::KeywordEnum;
}

bool Parser::is_declaration_start() const noexcept {
    return is_type_start() || (check(TokenType::KeywordStatic) && is_type_start(1));
}

const Token& Parser::peek(std::size_t offset) const noexcept {
    const std::size_t index = current + offset;
    if (index < tokens.size()) {
        return tokens[index];
    }
    return tokens.back();
}

const Token& Parser::previous() const noexcept {
    if (current == 0) {
        return tokens.front();
    }
    return tokens[current - 1];
}

const Token& Parser::advance() {
    if (!is_at_end()) {
        ++current;
    }
    return previous();
}

const Token& Parser::consume(TokenType type, std::string_view message) {
    if (check(type)) {
        return advance();
    }

    fail_current(std::string(message));
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

ExternalDeclaration Parser::parse_external_declaration() {
    DeclarationSpecifiers specifiers = parse_declaration_specifiers(true);

    if (match(TokenType::Semicolon)) {
        return VariableDeclaration{
            specifiers.location,
            specifiers.is_static,
            std::move(specifiers.type),
            {},
        };
    }

    Declarator declarator = parse_declarator(true);
    if (match(TokenType::LeftParen)) {
        FunctionDeclaration function{
            declarator.location,
            specifiers.is_static,
            std::move(specifiers.type),
            std::move(declarator),
            parse_parameter_list(),
        };
        consume(TokenType::RightParen, "expected ')' after function parameter list");

        if (check(TokenType::LeftBrace)) {
            return FunctionDefinition{
                std::move(function),
                parse_compound_statement(),
            };
        }

        consume(TokenType::Semicolon, "expected ';' after function declaration");
        return function;
    }

    VariableDeclaration declaration{
        specifiers.location,
        specifiers.is_static,
        std::move(specifiers.type),
        {},
    };
    declaration.declarators.push_back(finish_variable_declarator(std::move(declarator)));

    while (match(TokenType::Comma)) {
        declaration.declarators.push_back(parse_variable_declarator());
    }

    consume(TokenType::Semicolon, "expected ';' after declaration");
    return declaration;
}

Parser::DeclarationSpecifiers Parser::parse_declaration_specifiers(bool allow_static) {
    DeclarationSpecifiers specifiers;
    specifiers.location = location_from(peek());

    if (match(TokenType::KeywordStatic)) {
        if (!allow_static) {
            fail_at(previous(), "'static' is not valid in this declaration");
        }
        specifiers.is_static = true;
    }

    specifiers.type = parse_type_specifier();
    return specifiers;
}

TypePtr Parser::parse_type_specifier() {
    const SourceLocation location = location_from(peek());

    if (check(TokenType::KeywordStruct)) {
        return make_type(
            Type{
                location,
                {},
                parse_struct_specifier(),
                std::nullopt,
            }
        );
    }

    if (check(TokenType::KeywordEnum)) {
        return make_type(
            Type{
                location,
                {},
                std::nullopt,
                parse_enum_specifier(),
            }
        );
    }

    std::vector<PrimitiveType> primitive_specifiers;
    while (is_primitive_type_token(peek().type)) {
        primitive_specifiers.push_back(primitive_type_from(advance().type));
    }

    if (primitive_specifiers.empty()) {
        fail_current("expected a type specifier");
    }

    return make_type(
        Type{
            location,
            std::move(primitive_specifiers),
            std::nullopt,
            std::nullopt,
        }
    );
}

StructSpecifier Parser::parse_struct_specifier() {
    consume(TokenType::KeywordStruct, "expected 'struct'");

    StructSpecifier specifier;
    if (check(TokenType::Identifier)) {
        specifier.name = advance().lexeme;
    }

    if (match(TokenType::LeftBrace)) {
        specifier.has_definition = true;
        while (!check(TokenType::RightBrace)) {
            if (is_at_end()) {
                fail_current("expected '}' after struct fields");
            }

            if (match(TokenType::Semicolon)) {
                continue;
            }

            specifier.fields.push_back(parse_field_declaration());
        }
        consume(TokenType::RightBrace, "expected '}' after struct fields");
    }

    if (!specifier.name.has_value() && !specifier.has_definition) {
        fail_current("expected struct name or definition");
    }

    return specifier;
}

EnumSpecifier Parser::parse_enum_specifier() {
    consume(TokenType::KeywordEnum, "expected 'enum'");

    EnumSpecifier specifier;
    if (check(TokenType::Identifier)) {
        specifier.name = advance().lexeme;
    }

    if (match(TokenType::LeftBrace)) {
        specifier.has_definition = true;

        if (!check(TokenType::RightBrace)) {
            do {
                const Token& name = consume(TokenType::Identifier, "expected enumerator name");
                EnumEnumerator enumerator{
                    location_from(name),
                    name.lexeme,
                    nullptr,
                };

                if (match(TokenType::Equal)) {
                    enumerator.value = parse_expression();
                }

                specifier.enumerators.push_back(std::move(enumerator));
            } while (match(TokenType::Comma) && !check(TokenType::RightBrace));
        }

        consume(TokenType::RightBrace, "expected '}' after enum enumerators");
    }

    if (!specifier.name.has_value() && !specifier.has_definition) {
        fail_current("expected enum name or definition");
    }

    return specifier;
}

FieldDeclaration Parser::parse_field_declaration() {
    DeclarationSpecifiers specifiers = parse_declaration_specifiers(false);
    FieldDeclaration declaration{
        specifiers.location,
        std::move(specifiers.type),
        {},
    };

    if (!check(TokenType::Semicolon)) {
        declaration.declarators.push_back(parse_declarator(true));
        while (match(TokenType::Comma)) {
            declaration.declarators.push_back(parse_declarator(true));
        }
    }

    consume(TokenType::Semicolon, "expected ';' after field declaration");
    return declaration;
}

Declarator Parser::parse_declarator(bool require_name) {
    Declarator declarator;
    declarator.location = location_from(peek());

    while (match(TokenType::Star)) {
        ++declarator.pointer_depth;
    }

    if (check(TokenType::Identifier)) {
        declarator.name = advance().lexeme;
    } else if (require_name) {
        fail_current("expected declarator name");
    }

    while (match(TokenType::LeftBracket)) {
        ArrayDimension dimension;
        if (!check(TokenType::RightBracket)) {
            dimension.size = parse_expression();
        }
        consume(TokenType::RightBracket, "expected ']' after array dimension");
        declarator.array_dimensions.push_back(std::move(dimension));
    }

    return declarator;
}

VariableDeclaration Parser::parse_variable_declaration(bool allow_static) {
    DeclarationSpecifiers specifiers = parse_declaration_specifiers(allow_static);
    VariableDeclaration declaration{
        specifiers.location,
        specifiers.is_static,
        std::move(specifiers.type),
        {},
    };

    if (!check(TokenType::Semicolon)) {
        declaration.declarators.push_back(parse_variable_declarator());
        while (match(TokenType::Comma)) {
            declaration.declarators.push_back(parse_variable_declarator());
        }
    }

    consume(TokenType::Semicolon, "expected ';' after declaration");
    return declaration;
}

VariableDeclarator Parser::parse_variable_declarator() {
    return finish_variable_declarator(parse_declarator(true));
}

VariableDeclarator Parser::finish_variable_declarator(Declarator declarator) {
    VariableDeclarator variable{
        std::move(declarator),
        nullptr,
    };

    if (match(TokenType::Equal)) {
        variable.initializer = parse_initializer();
    }

    return variable;
}

std::vector<FunctionParameter> Parser::parse_parameter_list() {
    std::vector<FunctionParameter> parameters;

    if (check(TokenType::RightParen)) {
        return parameters;
    }

    if (check(TokenType::KeywordVoid) && check(TokenType::RightParen, 1)) {
        advance();
        return parameters;
    }

    do {
        parameters.push_back(parse_parameter());
    } while (match(TokenType::Comma));

    return parameters;
}

FunctionParameter Parser::parse_parameter() {
    DeclarationSpecifiers specifiers = parse_declaration_specifiers(false);
    Declarator declarator;
    declarator.location = location_from(peek());

    if (!check(TokenType::Comma) && !check(TokenType::RightParen)) {
        declarator = parse_declarator(false);
    }

    return FunctionParameter{
        specifiers.location,
        std::move(specifiers.type),
        std::move(declarator),
    };
}

InitializerPtr Parser::parse_initializer() {
    const SourceLocation location = location_from(peek());
    auto initializer = std::make_unique<Initializer>();
    initializer->location = location;

    if (match(TokenType::LeftBrace)) {
        if (!check(TokenType::RightBrace)) {
            do {
                initializer->values.push_back(parse_initializer());
            } while (match(TokenType::Comma) && !check(TokenType::RightBrace));
        }

        consume(TokenType::RightBrace, "expected '}' after initializer list");
        return initializer;
    }

    initializer->expression = parse_expression();
    return initializer;
}

StatementPtr Parser::parse_statement() {
    if (check(TokenType::LeftBrace)) {
        const SourceLocation location = location_from(peek());
        return make_statement(location, parse_compound_statement());
    }

    if (match(TokenType::Semicolon)) {
        return make_statement(location_from(previous()), EmptyStatement{});
    }

    if (check(TokenType::KeywordIf)) {
        return parse_if_statement();
    }

    if (check(TokenType::KeywordWhile)) {
        return parse_while_statement();
    }

    if (check(TokenType::KeywordFor)) {
        return parse_for_statement();
    }

    if (match(TokenType::KeywordBreak)) {
        const SourceLocation location = location_from(previous());
        consume(TokenType::Semicolon, "expected ';' after 'break'");
        return make_statement(location, BreakStatement{});
    }

    if (match(TokenType::KeywordContinue)) {
        const SourceLocation location = location_from(previous());
        consume(TokenType::Semicolon, "expected ';' after 'continue'");
        return make_statement(location, ContinueStatement{});
    }

    if (check(TokenType::KeywordReturn)) {
        return parse_return_statement();
    }

    if (check(TokenType::KeywordSwitch)) {
        return parse_switch_statement();
    }

    if (check(TokenType::KeywordCase)) {
        return parse_case_statement();
    }

    if (check(TokenType::KeywordDefault)) {
        return parse_default_statement();
    }

    if (is_declaration_start()) {
        const SourceLocation location = location_from(peek());
        return make_statement(location, DeclarationStatement{parse_variable_declaration(true)});
    }

    return parse_expression_statement();
}

CompoundStatement Parser::parse_compound_statement() {
    consume(TokenType::LeftBrace, "expected '{' before block");

    CompoundStatement compound;
    while (!check(TokenType::RightBrace)) {
        if (is_at_end()) {
            fail_current("expected '}' after block");
        }
        compound.statements.push_back(parse_statement());
    }

    consume(TokenType::RightBrace, "expected '}' after block");
    return compound;
}

StatementPtr Parser::parse_if_statement() {
    const Token& keyword = consume(TokenType::KeywordIf, "expected 'if'");
    consume(TokenType::LeftParen, "expected '(' after 'if'");
    ExpressionPtr condition = parse_expression();
    consume(TokenType::RightParen, "expected ')' after if condition");

    StatementPtr then_branch = parse_statement();
    StatementPtr else_branch;
    if (match(TokenType::KeywordElse)) {
        else_branch = parse_statement();
    }

    return make_statement(
        location_from(keyword),
        IfStatement{
            std::move(condition),
            std::move(then_branch),
            std::move(else_branch),
        }
    );
}

StatementPtr Parser::parse_while_statement() {
    const Token& keyword = consume(TokenType::KeywordWhile, "expected 'while'");
    consume(TokenType::LeftParen, "expected '(' after 'while'");
    ExpressionPtr condition = parse_expression();
    consume(TokenType::RightParen, "expected ')' after while condition");

    return make_statement(
        location_from(keyword),
        WhileStatement{
            std::move(condition),
            parse_statement(),
        }
    );
}

StatementPtr Parser::parse_for_statement() {
    const Token& keyword = consume(TokenType::KeywordFor, "expected 'for'");
    consume(TokenType::LeftParen, "expected '(' after 'for'");

    ForInitializer initializer;
    if (match(TokenType::Semicolon)) {
        initializer.value = std::monostate{};
    } else if (is_declaration_start()) {
        initializer.value = parse_variable_declaration(true);
    } else {
        initializer.value = parse_expression();
        consume(TokenType::Semicolon, "expected ';' after for initializer");
    }

    ExpressionPtr condition;
    if (!check(TokenType::Semicolon)) {
        condition = parse_expression();
    }
    consume(TokenType::Semicolon, "expected ';' after for condition");

    ExpressionPtr increment;
    if (!check(TokenType::RightParen)) {
        increment = parse_expression();
    }
    consume(TokenType::RightParen, "expected ')' after for clauses");

    return make_statement(
        location_from(keyword),
        ForStatement{
            std::move(initializer),
            std::move(condition),
            std::move(increment),
            parse_statement(),
        }
    );
}

StatementPtr Parser::parse_return_statement() {
    const Token& keyword = consume(TokenType::KeywordReturn, "expected 'return'");

    ExpressionPtr value;
    if (!check(TokenType::Semicolon)) {
        value = parse_expression();
    }

    consume(TokenType::Semicolon, "expected ';' after return statement");
    return make_statement(location_from(keyword), ReturnStatement{std::move(value)});
}

StatementPtr Parser::parse_switch_statement() {
    const Token& keyword = consume(TokenType::KeywordSwitch, "expected 'switch'");
    consume(TokenType::LeftParen, "expected '(' after 'switch'");
    ExpressionPtr expression = parse_expression();
    consume(TokenType::RightParen, "expected ')' after switch expression");

    return make_statement(
        location_from(keyword),
        SwitchStatement{
            std::move(expression),
            parse_statement(),
        }
    );
}

StatementPtr Parser::parse_case_statement() {
    const Token& keyword = consume(TokenType::KeywordCase, "expected 'case'");
    ExpressionPtr expression = parse_expression();
    consume(TokenType::Colon, "expected ':' after case expression");

    return make_statement(
        location_from(keyword),
        CaseStatement{
            std::move(expression),
            parse_statement(),
        }
    );
}

StatementPtr Parser::parse_default_statement() {
    const Token& keyword = consume(TokenType::KeywordDefault, "expected 'default'");
    consume(TokenType::Colon, "expected ':' after default label");

    return make_statement(
        location_from(keyword),
        DefaultStatement{
            parse_statement(),
        }
    );
}

StatementPtr Parser::parse_expression_statement() {
    const SourceLocation location = location_from(peek());
    ExpressionPtr expression = parse_expression();
    consume(TokenType::Semicolon, "expected ';' after expression");
    return make_statement(location, ExpressionStatement{std::move(expression)});
}

ExpressionPtr Parser::parse_expression() {
    return parse_assignment_expression();
}

ExpressionPtr Parser::parse_assignment_expression() {
    ExpressionPtr target = parse_conditional_expression();

    if (!is_assignment_token(peek().type)) {
        return target;
    }

    const Token& op = advance();
    return make_expression(
        location_from(op),
        AssignmentExpression{
            assignment_operator_from(op.type),
            std::move(target),
            parse_assignment_expression(),
        }
    );
}

ExpressionPtr Parser::parse_conditional_expression() {
    ExpressionPtr condition = parse_binary_expression(1);

    if (!match(TokenType::Question)) {
        return condition;
    }

    const SourceLocation location = location_from(previous());
    ExpressionPtr then_branch = parse_expression();
    consume(TokenType::Colon, "expected ':' in conditional expression");

    return make_expression(
        location,
        ConditionalExpression{
            std::move(condition),
            std::move(then_branch),
            parse_assignment_expression(),
        }
    );
}

ExpressionPtr Parser::parse_binary_expression(int min_precedence) {
    ExpressionPtr left = parse_unary_expression();

    while (true) {
        const int precedence = binary_precedence(peek().type);
        if (precedence < min_precedence) {
            break;
        }

        const Token& op = advance();
        ExpressionPtr right = parse_binary_expression(precedence + 1);
        left = make_expression(
            location_from(op),
            BinaryExpression{
                binary_operator_from(op.type),
                std::move(left),
                std::move(right),
            }
        );
    }

    return left;
}

ExpressionPtr Parser::parse_unary_expression() {
    const Token& token = peek();
    switch (token.type) {
    case TokenType::Plus:
        advance();
        return make_expression(location_from(token), UnaryExpression{UnaryOperator::Plus, parse_unary_expression()});
    case TokenType::Minus:
        advance();
        return make_expression(location_from(token), UnaryExpression{UnaryOperator::Minus, parse_unary_expression()});
    case TokenType::Bang:
        advance();
        return make_expression(
            location_from(token), UnaryExpression{UnaryOperator::LogicalNot, parse_unary_expression()}
        );
    case TokenType::Tilde:
        advance();
        return make_expression(
            location_from(token), UnaryExpression{UnaryOperator::BitwiseNot, parse_unary_expression()}
        );
    case TokenType::Ampersand:
        advance();
        return make_expression(
            location_from(token), UnaryExpression{UnaryOperator::AddressOf, parse_unary_expression()}
        );
    case TokenType::Star:
        advance();
        return make_expression(
            location_from(token), UnaryExpression{UnaryOperator::Dereference, parse_unary_expression()}
        );
    case TokenType::PlusPlus:
        advance();
        return make_expression(
            location_from(token), UnaryExpression{UnaryOperator::PreIncrement, parse_unary_expression()}
        );
    case TokenType::MinusMinus:
        advance();
        return make_expression(
            location_from(token), UnaryExpression{UnaryOperator::PreDecrement, parse_unary_expression()}
        );
    default:
        return parse_postfix_expression();
    }
}

ExpressionPtr Parser::parse_postfix_expression() {
    ExpressionPtr expression = parse_primary_expression();

    while (true) {
        if (match(TokenType::LeftParen)) {
            const SourceLocation location = location_from(previous());
            expression = make_expression(
                location,
                CallExpression{
                    std::move(expression),
                    parse_argument_list(),
                }
            );
            consume(TokenType::RightParen, "expected ')' after function arguments");
            continue;
        }

        if (match(TokenType::LeftBracket)) {
            const SourceLocation location = location_from(previous());
            ExpressionPtr index = parse_expression();
            consume(TokenType::RightBracket, "expected ']' after index expression");
            expression = make_expression(
                location,
                IndexExpression{
                    std::move(expression),
                    std::move(index),
                }
            );
            continue;
        }

        if (match(TokenType::Dot) || match(TokenType::Arrow)) {
            const Token& op = previous();
            const Token& member = consume(TokenType::Identifier, "expected member name");
            expression = make_expression(
                location_from(op),
                MemberExpression{
                    std::move(expression),
                    member.lexeme,
                    op.type == TokenType::Arrow,
                }
            );
            continue;
        }

        if (match(TokenType::PlusPlus)) {
            expression = make_expression(
                location_from(previous()),
                UnaryExpression{
                    UnaryOperator::PostIncrement,
                    std::move(expression),
                }
            );
            continue;
        }

        if (match(TokenType::MinusMinus)) {
            expression = make_expression(
                location_from(previous()),
                UnaryExpression{
                    UnaryOperator::PostDecrement,
                    std::move(expression),
                }
            );
            continue;
        }

        break;
    }

    return expression;
}

ExpressionPtr Parser::parse_primary_expression() {
    if (match(TokenType::Identifier)) {
        return make_expression(location_from(previous()), IdentifierExpression{previous().lexeme});
    }

    if (match(TokenType::IntegerLiteral)) {
        const Token& token = previous();
        return make_expression(
            location_from(token),
            LiteralExpression{
                LiteralKind::Integer,
                token.integer_value,
                token.lexeme,
                false,
            }
        );
    }

    if (match(TokenType::CharacterLiteral)) {
        const Token& token = previous();
        return make_expression(
            location_from(token),
            LiteralExpression{
                LiteralKind::Character,
                token.integer_value,
                token.text,
                false,
            }
        );
    }

    if (match(TokenType::StringLiteral)) {
        const Token& token = previous();
        return make_expression(
            location_from(token),
            LiteralExpression{
                LiteralKind::String,
                0,
                token.text,
                false,
            }
        );
    }

    if (match(TokenType::KeywordTrue) || match(TokenType::KeywordFalse)) {
        const Token& token = previous();
        return make_expression(
            location_from(token),
            LiteralExpression{
                LiteralKind::Boolean,
                token.type == TokenType::KeywordTrue ? 1U : 0U,
                token.lexeme,
                token.type == TokenType::KeywordTrue,
            }
        );
    }

    if (match(TokenType::LeftParen)) {
        ExpressionPtr expression = parse_expression();
        consume(TokenType::RightParen, "expected ')' after expression");
        return expression;
    }

    fail_current("expected expression");
}

std::vector<ExpressionPtr> Parser::parse_argument_list() {
    std::vector<ExpressionPtr> arguments;

    if (check(TokenType::RightParen)) {
        return arguments;
    }

    do {
        arguments.push_back(parse_expression());
    } while (match(TokenType::Comma));

    return arguments;
}

void Parser::fail_at(const Token& token, std::string message) const {
    throw std::runtime_error(
        "line " + std::to_string(token.line) + ", column " + std::to_string(token.column) + ": " + std::move(message) +
        " near " + token_description(token)
    );
}

void Parser::fail_current(std::string message) const {
    fail_at(peek(), std::move(message));
}

Ast parse(std::vector<Token> tokens) {
    return Parser(std::move(tokens)).parse();
}

std::string_view primitive_type_name(PrimitiveType type) noexcept {
    switch (type) {
    case PrimitiveType::Bool:
        return "bool";
    case PrimitiveType::Void:
        return "void";
    case PrimitiveType::Char:
        return "char";
    case PrimitiveType::Short:
        return "short";
    case PrimitiveType::Int:
        return "int";
    case PrimitiveType::Long:
        return "long";
    case PrimitiveType::Signed:
        return "signed";
    case PrimitiveType::Unsigned:
        return "unsigned";
    case PrimitiveType::Int8:
        return "int8_t";
    case PrimitiveType::Int16:
        return "int16_t";
    case PrimitiveType::Int32:
        return "int32_t";
    case PrimitiveType::Int64:
        return "int64_t";
    case PrimitiveType::UInt8:
        return "uint8_t";
    case PrimitiveType::UInt16:
        return "uint16_t";
    case PrimitiveType::UInt32:
        return "uint32_t";
    case PrimitiveType::UInt64:
        return "uint64_t";
    }

    return "unknown";
}

std::string_view unary_operator_name(UnaryOperator op) noexcept {
    switch (op) {
    case UnaryOperator::Plus:
        return "+";
    case UnaryOperator::Minus:
        return "-";
    case UnaryOperator::LogicalNot:
        return "!";
    case UnaryOperator::BitwiseNot:
        return "~";
    case UnaryOperator::AddressOf:
        return "&";
    case UnaryOperator::Dereference:
        return "*";
    case UnaryOperator::PreIncrement:
        return "pre++";
    case UnaryOperator::PreDecrement:
        return "pre--";
    case UnaryOperator::PostIncrement:
        return "post++";
    case UnaryOperator::PostDecrement:
        return "post--";
    }

    return "unknown";
}

std::string_view binary_operator_name(BinaryOperator op) noexcept {
    switch (op) {
    case BinaryOperator::Multiply:
        return "*";
    case BinaryOperator::Divide:
        return "/";
    case BinaryOperator::Modulo:
        return "%";
    case BinaryOperator::Add:
        return "+";
    case BinaryOperator::Subtract:
        return "-";
    case BinaryOperator::ShiftLeft:
        return "<<";
    case BinaryOperator::ShiftRight:
        return ">>";
    case BinaryOperator::Less:
        return "<";
    case BinaryOperator::LessEqual:
        return "<=";
    case BinaryOperator::Greater:
        return ">";
    case BinaryOperator::GreaterEqual:
        return ">=";
    case BinaryOperator::Equal:
        return "==";
    case BinaryOperator::NotEqual:
        return "!=";
    case BinaryOperator::BitwiseAnd:
        return "&";
    case BinaryOperator::BitwiseXor:
        return "^";
    case BinaryOperator::BitwiseOr:
        return "|";
    case BinaryOperator::LogicalAnd:
        return "&&";
    case BinaryOperator::LogicalOr:
        return "||";
    }

    return "unknown";
}

std::string_view assignment_operator_name(AssignmentOperator op) noexcept {
    switch (op) {
    case AssignmentOperator::Assign:
        return "=";
    case AssignmentOperator::AddAssign:
        return "+=";
    case AssignmentOperator::SubtractAssign:
        return "-=";
    case AssignmentOperator::MultiplyAssign:
        return "*=";
    case AssignmentOperator::DivideAssign:
        return "/=";
    case AssignmentOperator::ModuloAssign:
        return "%=";
    case AssignmentOperator::ShiftLeftAssign:
        return "<<=";
    case AssignmentOperator::ShiftRightAssign:
        return ">>=";
    case AssignmentOperator::BitwiseAndAssign:
        return "&=";
    case AssignmentOperator::BitwiseXorAssign:
        return "^=";
    case AssignmentOperator::BitwiseOrAssign:
        return "|=";
    }

    return "unknown";
}

} // namespace zenith::compiler
