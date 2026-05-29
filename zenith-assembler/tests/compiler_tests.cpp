#include "test_utils.hpp"

#include "zenith/compiler.hpp"
#include "zenith/parser.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace zenith::assembler;

TEST(Compiler, EncodesInstructionsAndPcRelativeLabels) {
    const Ast ast = test::parse_source(R"(.main
start: add r1, r2, r3
  addi a0, zero, 42
  beq r1, r2, start
  j start
.data
  value: .byte 100
)");

    const std::vector<std::uint32_t> code = Compiler(ast).compile();

    ASSERT_EQ(code.size(), 4);

    EXPECT_EQ(code[0] & 0x7F, 0x00U);
    EXPECT_EQ(test::field1(code[0]), 1);
    EXPECT_EQ(test::field2(code[0]), 2);
    EXPECT_EQ(test::field3(code[0]), 3);

    EXPECT_EQ(code[1] & 0x7F, 0x04);
    EXPECT_EQ(test::field1(code[1]), 3);
    EXPECT_EQ(test::field2(code[1]), 0);
    EXPECT_EQ(test::imm15(code[1]), 42);

    EXPECT_EQ(code[2] & 0x7F, 0x14);
    EXPECT_EQ(test::field1(code[2]), 1);
    EXPECT_EQ(test::field2(code[2]), 2);
    EXPECT_EQ(test::imm15(code[2]), -8);

    EXPECT_EQ(code[3] & 0x7F, 0x1A);
    EXPECT_EQ(test::field1(code[3]), 0);
    EXPECT_EQ(test::imm20(code[3]), -12);
}

TEST(Compiler, RejectsDuplicateLabels) {
    const Ast ast = test::parse_source(R"(.main
same: add r0, r0, r0
same: add r1, r1, r1
)");

    EXPECT_THROW(static_cast<void>(Compiler(ast).compile()), std::runtime_error);
}

TEST(Compiler, EncodesBitwiseAndShiftInstructions) {
    const Ast ast = test::parse_source(R"(.main
  not r1, r2
  sll r3, r4, r5
  srl r6, r7, r8
  sla r9, r10, r11
  sra r12, r13, r14
)");

    const std::vector<std::uint32_t> code = Compiler(ast).compile();

    ASSERT_EQ(code.size(), 5);

    EXPECT_EQ(code[0] & 0x7F, 0x1CU);
    EXPECT_EQ(test::field1(code[0]), 1);
    EXPECT_EQ(test::field2(code[0]), 2);
    EXPECT_EQ(test::field3(code[0]), 0);

    EXPECT_EQ(code[1] & 0x7F, 0x1DU);
    EXPECT_EQ(test::field1(code[1]), 3);
    EXPECT_EQ(test::field2(code[1]), 4);
    EXPECT_EQ(test::field3(code[1]), 5);

    EXPECT_EQ(code[2] & 0x7F, 0x1EU);
    EXPECT_EQ(test::field1(code[2]), 6);
    EXPECT_EQ(test::field2(code[2]), 7);
    EXPECT_EQ(test::field3(code[2]), 8);

    EXPECT_EQ(code[3] & 0x7F, 0x1FU);
    EXPECT_EQ(test::field1(code[3]), 9);
    EXPECT_EQ(test::field2(code[3]), 10);
    EXPECT_EQ(test::field3(code[3]), 11);

    EXPECT_EQ(code[4] & 0x7F, 0x20U);
    EXPECT_EQ(test::field1(code[4]), 12);
    EXPECT_EQ(test::field2(code[4]), 13);
    EXPECT_EQ(test::field3(code[4]), 14);
}

} // namespace
