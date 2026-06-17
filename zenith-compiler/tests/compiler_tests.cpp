#include "zenith/compiler.hpp"

#include <array>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using namespace zenith::compiler;

TEST(Compiler, ExampleTest) {
    Compiler compiler;
    EXPECT_EQ(compiler.version(), "0.1.0");
}

TEST(Compiler, CompilesSourceThroughPipelineToAssembly) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
#define ANSWER 42

int main() {
  return ANSWER;
}
)");

    EXPECT_NE(assembly.find(".main"), std::string::npos);
    EXPECT_NE(assembly.find("start:"), std::string::npos);
    EXPECT_NE(assembly.find("jal ra, F0"), std::string::npos);
    EXPECT_NE(assembly.find("addi t0, zero, 42"), std::string::npos);
    EXPECT_NE(assembly.find("jalr zero, ra, 0"), std::string::npos);
}

TEST(Compiler, CompilesControlFlowLocalsAndCalls) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
int add(int a, int b) {
  return a + b;
}

int main() {
  int total = 0;
  for (int i = 0; i < 4; i = i + 1) {
    total = add(total, i);
  }
  if (total == 6) {
    return total;
  }
  return 0;
}
)");

    EXPECT_NE(assembly.find("jal ra, F0"), std::string::npos);
    EXPECT_NE(assembly.find("jal ra, F1"), std::string::npos);
    EXPECT_NE(assembly.find("blt"), std::string::npos);
    EXPECT_NE(assembly.find("beq"), std::string::npos);
    EXPECT_NE(assembly.find("s64"), std::string::npos);
    EXPECT_NE(assembly.find("l64"), std::string::npos);
}

TEST(Compiler, EmitsComparisonsBeforeWritingBooleanResult) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
int main() {
  int a = 5;
  int b = 7;
  int total = 0;
  total = total + (a < b);
  total = total + (a <= b);
  total = total + (a > b);
  total = total + (a >= b);
  total = total + (a == b);
  total = total + (a != b);
  return total;
}
)");

    for (const char* mnemonic : std::array{"blt", "ble", "bgt", "bge", "beq", "bne"}) {
        const std::string branch = std::string("  ") + mnemonic + " t1, t0,";
        EXPECT_NE(assembly.find(branch), std::string::npos) << mnemonic;

        const std::string clobber_before_branch = std::string("  addi t0, zero, 0\n") + branch;
        EXPECT_EQ(assembly.find(clobber_before_branch), std::string::npos) << mnemonic;
    }
}

TEST(Compiler, CompilesGlobalScalarsAndArraysToDataSection) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
enum Mode { MODE_A = 7 };
int global = MODE_A;
int values[] = {1, 2};

void bump() {
  global += values[1];
}

int main() {
  int global = 3;
  bump();
  values[0] = *(&values[0]) + global;
  return values[0];
}
)");

    EXPECT_NE(assembly.find(".data"), std::string::npos);
    EXPECT_NE(assembly.find("G0:"), std::string::npos);
    EXPECT_NE(assembly.find("  .quad 7"), std::string::npos);
    EXPECT_NE(assembly.find("  .quad 1"), std::string::npos);
    EXPECT_NE(assembly.find("  .quad 2"), std::string::npos);
    EXPECT_NE(assembly.find(".main"), std::string::npos);
    EXPECT_NE(assembly.find("l64"), std::string::npos);
    EXPECT_NE(assembly.find("s64"), std::string::npos);
}

TEST(Compiler, PacksByteSizedGlobalArrays) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
uint8_t bytes[] = {0x00, 0x7F, 0x80, 0xFF};

int main() {
  return bytes[3];
}
)");

    EXPECT_NE(assembly.find("  .byte 0"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 127"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 128"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 255"), std::string::npos);
    EXPECT_EQ(assembly.find("  .quad 255"), std::string::npos);
    EXPECT_NE(assembly.find("l8"), std::string::npos);
    EXPECT_NE(assembly.find("and"), std::string::npos);
}

TEST(Compiler, EmitsStringLiteralsAsNullTerminatedData) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
char *message = "hi\n";

int main() {
  return message[1];
}
)");

    EXPECT_NE(assembly.find(".data"), std::string::npos);
    EXPECT_NE(assembly.find("S0:"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 104"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 105"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 10"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 0"), std::string::npos);
    EXPECT_NE(assembly.find("  .quad 8"), std::string::npos);
    EXPECT_NE(assembly.find("l8"), std::string::npos);
}

TEST(Compiler, InitializesCharArraysFromStringLiterals) {
    Compiler compiler;

    const std::string assembly = compiler.compile(R"(
char global[] = "ok";

int main() {
  char local[4] = "go";
  return global[1] + local[0];
}
)");

    EXPECT_NE(assembly.find("G0:"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 111"), std::string::npos);
    EXPECT_NE(assembly.find("  .byte 107"), std::string::npos);
    EXPECT_NE(assembly.find("s8"), std::string::npos);
    EXPECT_NE(assembly.find("l8"), std::string::npos);
}

TEST(Compiler, RejectsUnsupportedGlobalForms) {
    Compiler compiler;

    EXPECT_THROW(static_cast<void>(compiler.compile("static int global; int main() { return 0; }")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(compiler.compile("int global = main(); int main() { return 0; }")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(compiler.compile("struct Pair { int x; } pair; int main() { return 0; }")), std::runtime_error);
}

} // namespace
