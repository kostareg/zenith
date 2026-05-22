#include "zenith/assembler.hpp"

#include <string_view>

#include <gtest/gtest.h>

namespace {

TEST(Assembler, ExposesLibraryVersion) {
    EXPECT_EQ(zenith::assembler::Assembler::version(), std::string_view("0.1.0"));
}

} // namespace
