#pragma once

#include "zenith/lexer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace zenith::compiler {

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Expression;
struct Initializer;
struct Statement;
struct Type;

using ExpressionPtr = std::unique_ptr<Expression>;
using InitializerPtr = std::unique_ptr<Initializer>;
using StatementPtr = std::unique_ptr<Statement>;
using TypePtr = std::unique_ptr<Type>;

enum class PrimitiveType {
    Bool,
    Void,
    Char,
    Short,
    Int,
    Long,
    Signed,
    Unsigned,
    Int8,
    Int16,
    Int32,
    Int64,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
};

enum class LiteralKind {
    Integer,
    Character,
    String,
    Boolean,
};

enum class UnaryOperator {
    Plus,
    Minus,
    LogicalNot,
    BitwiseNot,
    AddressOf,
    Dereference,
    PreIncrement,
    PreDecrement,
    PostIncrement,
    PostDecrement,
};

enum class BinaryOperator {
    Multiply,
    Divide,
    Modulo,
    Add,
    Subtract,
    ShiftLeft,
    ShiftRight,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    BitwiseAnd,
    BitwiseXor,
    BitwiseOr,
    LogicalAnd,
    LogicalOr,
};

enum class AssignmentOperator {
    Assign,
    AddAssign,
    SubtractAssign,
    MultiplyAssign,
    DivideAssign,
    ModuloAssign,
    ShiftLeftAssign,
    ShiftRightAssign,
    BitwiseAndAssign,
    BitwiseXorAssign,
    BitwiseOrAssign,
};

struct ArrayDimension {
    ExpressionPtr size;
};

struct Declarator {
    SourceLocation location;
    std::optional<std::string> name;
    std::size_t pointer_depth = 0;
    std::vector<ArrayDimension> array_dimensions;
};

struct FieldDeclaration {
    SourceLocation location;
    TypePtr type;
    std::vector<Declarator> declarators;
};

struct StructSpecifier {
    std::optional<std::string> name;
    bool has_definition = false;
    std::vector<FieldDeclaration> fields;
};

struct EnumEnumerator {
    SourceLocation location;
    std::string name;
    ExpressionPtr value;
};

struct EnumSpecifier {
    std::optional<std::string> name;
    bool has_definition = false;
    std::vector<EnumEnumerator> enumerators;
};

struct Type {
    SourceLocation location;
    std::vector<PrimitiveType> primitive_specifiers;
    std::optional<StructSpecifier> struct_specifier;
    std::optional<EnumSpecifier> enum_specifier;
};

struct LiteralExpression {
    LiteralKind kind = LiteralKind::Integer;
    std::uint64_t integer_value = 0;
    std::string text;
    bool boolean_value = false;
};

struct IdentifierExpression {
    std::string name;
};

struct UnaryExpression {
    UnaryOperator op = UnaryOperator::Plus;
    ExpressionPtr operand;
};

struct BinaryExpression {
    BinaryOperator op = BinaryOperator::Add;
    ExpressionPtr left;
    ExpressionPtr right;
};

struct AssignmentExpression {
    AssignmentOperator op = AssignmentOperator::Assign;
    ExpressionPtr target;
    ExpressionPtr value;
};

struct ConditionalExpression {
    ExpressionPtr condition;
    ExpressionPtr then_branch;
    ExpressionPtr else_branch;
};

struct CallExpression {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
};

struct IndexExpression {
    ExpressionPtr object;
    ExpressionPtr index;
};

struct MemberExpression {
    ExpressionPtr object;
    std::string member;
    bool pointer_access = false;
};

struct Expression {
    using Node = std::variant<
        LiteralExpression,
        IdentifierExpression,
        UnaryExpression,
        BinaryExpression,
        AssignmentExpression,
        ConditionalExpression,
        CallExpression,
        IndexExpression,
        MemberExpression>;

    SourceLocation location;
    Node node;
};

struct Initializer {
    SourceLocation location;
    ExpressionPtr expression;
    std::vector<InitializerPtr> values;

    [[nodiscard]] bool is_list() const noexcept {
        return expression == nullptr;
    }
};

struct VariableDeclarator {
    Declarator declarator;
    InitializerPtr initializer;
};

struct VariableDeclaration {
    SourceLocation location;
    bool is_static = false;
    TypePtr type;
    std::vector<VariableDeclarator> declarators;
};

struct DeclarationStatement {
    VariableDeclaration declaration;
};

struct EmptyStatement {};

struct ExpressionStatement {
    ExpressionPtr expression;
};

struct CompoundStatement {
    std::vector<StatementPtr> statements;
};

struct IfStatement {
    ExpressionPtr condition;
    StatementPtr then_branch;
    StatementPtr else_branch;
};

struct WhileStatement {
    ExpressionPtr condition;
    StatementPtr body;
};

struct ForInitializer {
    std::variant<std::monostate, VariableDeclaration, ExpressionPtr> value;
};

struct ForStatement {
    ForInitializer initializer;
    ExpressionPtr condition;
    ExpressionPtr increment;
    StatementPtr body;
};

struct BreakStatement {};

struct ContinueStatement {};

struct ReturnStatement {
    ExpressionPtr value;
};

struct SwitchStatement {
    ExpressionPtr expression;
    StatementPtr body;
};

struct CaseStatement {
    ExpressionPtr expression;
    StatementPtr body;
};

struct DefaultStatement {
    StatementPtr body;
};

struct Statement {
    using Node = std::variant<
        EmptyStatement,
        ExpressionStatement,
        DeclarationStatement,
        CompoundStatement,
        IfStatement,
        WhileStatement,
        ForStatement,
        BreakStatement,
        ContinueStatement,
        ReturnStatement,
        SwitchStatement,
        CaseStatement,
        DefaultStatement>;

    SourceLocation location;
    Node node;
};

struct FunctionParameter {
    SourceLocation location;
    TypePtr type;
    Declarator declarator;
};

struct FunctionDeclaration {
    SourceLocation location;
    bool is_static = false;
    TypePtr return_type;
    Declarator declarator;
    std::vector<FunctionParameter> parameters;
};

struct FunctionDefinition {
    FunctionDeclaration declaration;
    CompoundStatement body;
};

using ExternalDeclaration = std::variant<VariableDeclaration, FunctionDeclaration, FunctionDefinition>;

struct Ast {
    std::vector<ExternalDeclaration> declarations;
};

class Parser {
  public:
    Parser() = delete;
    explicit Parser(std::vector<Token> tokens);

    [[nodiscard]] Ast parse();

  private:
    struct DeclarationSpecifiers {
        SourceLocation location;
        bool is_static = false;
        TypePtr type;
    };

    std::vector<Token> tokens;
    std::size_t current = 0;

    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] bool check(TokenType type, std::size_t offset = 0) const noexcept;
    [[nodiscard]] bool is_type_start(std::size_t offset = 0) const noexcept;
    [[nodiscard]] bool is_declaration_start() const noexcept;
    [[nodiscard]] const Token& peek(std::size_t offset = 0) const noexcept;
    [[nodiscard]] const Token& previous() const noexcept;

    const Token& advance();
    const Token& consume(TokenType type, std::string_view message);
    bool match(TokenType type);

    [[nodiscard]] ExternalDeclaration parse_external_declaration();
    [[nodiscard]] DeclarationSpecifiers parse_declaration_specifiers(bool allow_static);
    [[nodiscard]] TypePtr parse_type_specifier();
    [[nodiscard]] StructSpecifier parse_struct_specifier();
    [[nodiscard]] EnumSpecifier parse_enum_specifier();
    [[nodiscard]] FieldDeclaration parse_field_declaration();
    [[nodiscard]] Declarator parse_declarator(bool require_name);
    [[nodiscard]] VariableDeclaration parse_variable_declaration(bool allow_static);
    [[nodiscard]] VariableDeclarator parse_variable_declarator();
    [[nodiscard]] VariableDeclarator finish_variable_declarator(Declarator declarator);
    [[nodiscard]] std::vector<FunctionParameter> parse_parameter_list();
    [[nodiscard]] FunctionParameter parse_parameter();
    [[nodiscard]] InitializerPtr parse_initializer();

    [[nodiscard]] StatementPtr parse_statement();
    [[nodiscard]] CompoundStatement parse_compound_statement();
    [[nodiscard]] StatementPtr parse_if_statement();
    [[nodiscard]] StatementPtr parse_while_statement();
    [[nodiscard]] StatementPtr parse_for_statement();
    [[nodiscard]] StatementPtr parse_return_statement();
    [[nodiscard]] StatementPtr parse_switch_statement();
    [[nodiscard]] StatementPtr parse_case_statement();
    [[nodiscard]] StatementPtr parse_default_statement();
    [[nodiscard]] StatementPtr parse_expression_statement();

    [[nodiscard]] ExpressionPtr parse_expression();
    [[nodiscard]] ExpressionPtr parse_assignment_expression();
    [[nodiscard]] ExpressionPtr parse_conditional_expression();
    [[nodiscard]] ExpressionPtr parse_binary_expression(int min_precedence);
    [[nodiscard]] ExpressionPtr parse_unary_expression();
    [[nodiscard]] ExpressionPtr parse_postfix_expression();
    [[nodiscard]] ExpressionPtr parse_primary_expression();
    [[nodiscard]] std::vector<ExpressionPtr> parse_argument_list();

    [[noreturn]] void fail_at(const Token& token, std::string message) const;
    [[noreturn]] void fail_current(std::string message) const;
};

[[nodiscard]] Ast parse(std::vector<Token> tokens);

[[nodiscard]] std::string_view primitive_type_name(PrimitiveType type) noexcept;
[[nodiscard]] std::string_view unary_operator_name(UnaryOperator op) noexcept;
[[nodiscard]] std::string_view binary_operator_name(BinaryOperator op) noexcept;
[[nodiscard]] std::string_view assignment_operator_name(AssignmentOperator op) noexcept;

} // namespace zenith::compiler
