#include "zenith/format.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace zenith::assembler {

constexpr std::size_t kHeaderSize = 40;
constexpr std::size_t kMaxNameLength = 8;

void append_u32_le(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU));
    }
}

void append_u64_le(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU));
    }
}

void append_name(std::vector<std::uint8_t>& bytes, std::string_view name) {
    for (const char ch : name) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }

    for (std::size_t i = name.size(); i < kMaxNameLength; ++i) {
        bytes.push_back(0);
    }
}

std::vector<std::uint8_t> Format::format() const {
    if (name.size() > kMaxNameLength) {
        throw std::runtime_error("Zelf executable name must be 8 bytes or fewer");
    }

    if constexpr (std::numeric_limits<std::size_t>::max() > std::numeric_limits<std::uint64_t>::max() / 4) {
        if (image.code.size() > std::numeric_limits<std::uint64_t>::max() / 4) {
            throw std::runtime_error("Zelf code section is too large");
        }
    }

    if (image.code.size() > (std::numeric_limits<std::size_t>::max() - kHeaderSize - image.data.size()) / 4) {
        throw std::runtime_error("Zelf output is too large");
    }

    const std::uint64_t data_size = static_cast<std::uint64_t>(image.data.size());
    const std::uint64_t code_size = static_cast<std::uint64_t>(image.code.size()) * 4;
    const std::uint64_t entry_off = image.entry_offset;

    std::vector<std::uint8_t> bytes;
    bytes.reserve(kHeaderSize + image.data.size() + image.code.size() * 4);

    bytes.push_back('Z');
    bytes.push_back('E');
    bytes.push_back('L');
    bytes.push_back('F');
    bytes.push_back(static_cast<std::uint8_t>(name.size()));
    bytes.push_back(0);
    bytes.push_back(0);
    bytes.push_back(0);
    append_name(bytes, name);
    append_u64_le(bytes, data_size);
    append_u64_le(bytes, code_size);
    append_u64_le(bytes, entry_off);

    bytes.insert(bytes.end(), image.data.begin(), image.data.end());

    for (const std::uint32_t instruction : image.code) {
        append_u32_le(bytes, instruction);
    }

    return bytes;
}

} // namespace zenith::assembler
