#include "zenith/compiler.hpp"

#include <gtest/gtest.h>

namespace {

using namespace zenith::compiler;

TEST(Compiler, ExampleTest) {
    Compiler compiler;
    EXPECT_EQ(compiler.version(), "0.1.0");
}

} // namespace
