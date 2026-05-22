#include "test_utils.hpp"

#include "zenith/parser.hpp"

#include <variant>

#include <gtest/gtest.h>

namespace {

using namespace zenith::assembler;

TEST(Parser, BuildsSectionsStatementsLabelsAndDirectives) {
    const Ast ast = test::parse_source(R"(.main
  add r0, r0, r1
  mylabel: add r1, r1, r1
  j mylabel

.data
  label: .byte 100
)");

    ASSERT_EQ(ast.sections.size(), 2);

    const Section& main = ast.sections[0];
    EXPECT_EQ(main.name, "main");
    ASSERT_EQ(main.statements.size(), 3);

    ASSERT_TRUE(main.statements[0].operation.has_value());
    const auto* first_instruction = std::get_if<Instruction>(&*main.statements[0].operation);
    ASSERT_NE(first_instruction, nullptr);
    EXPECT_EQ(first_instruction->mnemonic, "add");
    ASSERT_EQ(first_instruction->operands.size(), 3);
    EXPECT_EQ(first_instruction->operands[2].kind, OperandKind::Identifier);
    EXPECT_EQ(first_instruction->operands[2].ident, "r1");

    ASSERT_TRUE(main.statements[1].label.has_value());
    EXPECT_EQ(*main.statements[1].label, "mylabel");

    const Section& data = ast.sections[1];
    EXPECT_EQ(data.name, "data");
    ASSERT_EQ(data.statements.size(), 1);
    ASSERT_TRUE(data.statements[0].label.has_value());
    EXPECT_EQ(*data.statements[0].label, "label");
    ASSERT_TRUE(data.statements[0].operation.has_value());

    const auto* directive = std::get_if<Directive>(&*data.statements[0].operation);
    ASSERT_NE(directive, nullptr);
    EXPECT_EQ(directive->name, "byte");
    ASSERT_EQ(directive->operands.size(), 1);
    EXPECT_EQ(directive->operands[0].kind, OperandKind::Number);
    EXPECT_EQ(directive->operands[0].number, 100);
}

} // namespace
