#include "zenith/format.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace zenith::assembler;

TEST(Format, WritesZelfHeaderAndLittleEndianCode) {
    const std::vector<std::uint32_t> code{0x00020000, 0xffffc01a};
    const std::vector<std::uint8_t> bytes = Format("demo", code).format();

    ASSERT_EQ(bytes.size(), 48);

    EXPECT_EQ(bytes[0], 'Z');
    EXPECT_EQ(bytes[1], 'E');
    EXPECT_EQ(bytes[2], 'L');
    EXPECT_EQ(bytes[3], 'F');
    EXPECT_EQ(bytes[4], 4);
    EXPECT_EQ(bytes[5], 0);
    EXPECT_EQ(bytes[6], 0);
    EXPECT_EQ(bytes[7], 0);

    EXPECT_EQ(bytes[8], 'd');
    EXPECT_EQ(bytes[9], 'e');
    EXPECT_EQ(bytes[10], 'm');
    EXPECT_EQ(bytes[11], 'o');
    for (std::size_t i = 12; i < 16; ++i) {
        EXPECT_EQ(bytes[i], 0);
    }

    for (std::size_t i = 16; i < 24; ++i) {
        EXPECT_EQ(bytes[i], 0);
    }

    EXPECT_EQ(bytes[24], 8);
    for (std::size_t i = 25; i < 40; ++i) {
        EXPECT_EQ(bytes[i], 0);
    }

    EXPECT_EQ(bytes[40], 0x00);
    EXPECT_EQ(bytes[41], 0x00);
    EXPECT_EQ(bytes[42], 0x02);
    EXPECT_EQ(bytes[43], 0x00);
    EXPECT_EQ(bytes[44], 0x1a);
    EXPECT_EQ(bytes[45], 0xc0);
    EXPECT_EQ(bytes[46], 0xff);
    EXPECT_EQ(bytes[47], 0xff);
}

TEST(Format, RejectsNamesLongerThanZelfHeaderAllows) {
    EXPECT_THROW(static_cast<void>(Format("toolonggg", {}).format()), std::runtime_error);
}

} // namespace
