#include "zenith/lexer.hpp"
#include "zenith/parser.hpp"

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace zenith::compiler;

Ast parse_source(std::string source) {
    return Parser(Lexer(std::move(source)).lex()).parse();
}

TEST(Parser, BuildsFunctionDefinitionsStatementsAndExpressionPrecedence) {
    Ast ast = parse_source(R"(
int add(int a, int b) {
  int result = a + b * 2;
  if (result > 10) {
    return result;
  }
  return result ? result : 0;
}
)");

    ASSERT_EQ(ast.declarations.size(), 1);
    const auto* function = std::get_if<FunctionDefinition>(&ast.declarations[0]);
    ASSERT_NE(function, nullptr);

    ASSERT_TRUE(function->declaration.declarator.name.has_value());
    EXPECT_EQ(*function->declaration.declarator.name, "add");
    ASSERT_EQ(function->declaration.parameters.size(), 2);
    ASSERT_TRUE(function->declaration.parameters[0].declarator.name.has_value());
    EXPECT_EQ(*function->declaration.parameters[0].declarator.name, "a");

    ASSERT_EQ(function->body.statements.size(), 3);
    const auto* declaration_statement = std::get_if<DeclarationStatement>(&function->body.statements[0]->node);
    ASSERT_NE(declaration_statement, nullptr);
    ASSERT_EQ(declaration_statement->declaration.declarators.size(), 1);
    ASSERT_NE(declaration_statement->declaration.declarators[0].initializer, nullptr);

    const Expression& initializer = *declaration_statement->declaration.declarators[0].initializer->expression;
    const auto* add_expression = std::get_if<BinaryExpression>(&initializer.node);
    ASSERT_NE(add_expression, nullptr);
    EXPECT_EQ(add_expression->op, BinaryOperator::Add);
    ASSERT_NE(std::get_if<BinaryExpression>(&add_expression->right->node), nullptr);
    EXPECT_EQ(std::get<BinaryExpression>(add_expression->right->node).op, BinaryOperator::Multiply);

    ASSERT_NE(std::get_if<IfStatement>(&function->body.statements[1]->node), nullptr);

    const auto* return_statement = std::get_if<ReturnStatement>(&function->body.statements[2]->node);
    ASSERT_NE(return_statement, nullptr);
    ASSERT_NE(return_statement->value, nullptr);
    ASSERT_NE(std::get_if<ConditionalExpression>(&return_statement->value->node), nullptr);
}

TEST(Parser, BuildsGlobalsStructsEnumsAndFunctionDeclarations) {
    Ast ast = parse_source(R"(
static unsigned int counter = 1;
struct Pair {
  int left;
  int right;
};
enum Mode {
  MODE_A = 1,
  MODE_B,
};
int *lookup(int key);
)");

    ASSERT_EQ(ast.declarations.size(), 4);

    const auto* counter = std::get_if<VariableDeclaration>(&ast.declarations[0]);
    ASSERT_NE(counter, nullptr);
    EXPECT_TRUE(counter->is_static);
    ASSERT_NE(counter->type, nullptr);
    ASSERT_EQ(counter->type->primitive_specifiers.size(), 2);
    EXPECT_EQ(counter->type->primitive_specifiers[0], PrimitiveType::Unsigned);
    EXPECT_EQ(counter->type->primitive_specifiers[1], PrimitiveType::Int);
    ASSERT_EQ(counter->declarators.size(), 1);
    ASSERT_TRUE(counter->declarators[0].declarator.name.has_value());
    EXPECT_EQ(*counter->declarators[0].declarator.name, "counter");

    const auto* pair = std::get_if<VariableDeclaration>(&ast.declarations[1]);
    ASSERT_NE(pair, nullptr);
    ASSERT_NE(pair->type, nullptr);
    ASSERT_TRUE(pair->type->struct_specifier.has_value());
    EXPECT_TRUE(pair->type->struct_specifier->has_definition);
    ASSERT_EQ(pair->type->struct_specifier->fields.size(), 2);
    ASSERT_EQ(pair->type->struct_specifier->fields[0].declarators.size(), 1);
    EXPECT_EQ(*pair->type->struct_specifier->fields[0].declarators[0].name, "left");

    const auto* mode = std::get_if<VariableDeclaration>(&ast.declarations[2]);
    ASSERT_NE(mode, nullptr);
    ASSERT_NE(mode->type, nullptr);
    ASSERT_TRUE(mode->type->enum_specifier.has_value());
    ASSERT_EQ(mode->type->enum_specifier->enumerators.size(), 2);
    EXPECT_EQ(mode->type->enum_specifier->enumerators[0].name, "MODE_A");

    const auto* lookup = std::get_if<FunctionDeclaration>(&ast.declarations[3]);
    ASSERT_NE(lookup, nullptr);
    ASSERT_TRUE(lookup->declarator.name.has_value());
    EXPECT_EQ(*lookup->declarator.name, "lookup");
    EXPECT_EQ(lookup->declarator.pointer_depth, 1);
    ASSERT_EQ(lookup->parameters.size(), 1);
    ASSERT_TRUE(lookup->parameters[0].declarator.name.has_value());
    EXPECT_EQ(*lookup->parameters[0].declarator.name, "key");
}

TEST(Parser, BuildsForSwitchCaseAndDefaultStatements) {
    Ast ast = parse_source(R"(
void run(int n) {
  for (int i = 0; i < n; i = i + 1) {
    switch (i) {
      case 0: continue;
      default: break;
    }
  }
}
)");

    ASSERT_EQ(ast.declarations.size(), 1);
    const auto* function = std::get_if<FunctionDefinition>(&ast.declarations[0]);
    ASSERT_NE(function, nullptr);
    ASSERT_EQ(function->body.statements.size(), 1);

    const auto* for_statement = std::get_if<ForStatement>(&function->body.statements[0]->node);
    ASSERT_NE(for_statement, nullptr);
    ASSERT_NE(std::get_if<VariableDeclaration>(&for_statement->initializer.value), nullptr);
    ASSERT_NE(for_statement->condition, nullptr);
    ASSERT_NE(for_statement->increment, nullptr);

    const auto* for_body = std::get_if<CompoundStatement>(&for_statement->body->node);
    ASSERT_NE(for_body, nullptr);
    ASSERT_EQ(for_body->statements.size(), 1);

    const auto* switch_statement = std::get_if<SwitchStatement>(&for_body->statements[0]->node);
    ASSERT_NE(switch_statement, nullptr);
    const auto* switch_body = std::get_if<CompoundStatement>(&switch_statement->body->node);
    ASSERT_NE(switch_body, nullptr);
    ASSERT_EQ(switch_body->statements.size(), 2);
    ASSERT_NE(std::get_if<CaseStatement>(&switch_body->statements[0]->node), nullptr);
    ASSERT_NE(std::get_if<DefaultStatement>(&switch_body->statements[1]->node), nullptr);
}

TEST(Parser, ReportsSyntaxErrorsWithLocations) {
    EXPECT_THROW(parse_source("int main( { return 0; }"), std::runtime_error);
}

} // namespace
