#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace zenith::emulator {

class Emulator {
  public:
    Emulator() = default;

    enum class Operator : std::uint8_t {
        Add = 0x00,
        Sub = 0x01,
        Mul = 0x02,
        Div = 0x03,
        Addi = 0x04,
        Muli = 0x05,
        Divi = 0x06,
        BitAnd = 0x07,
        BitOr = 0x08,
        BitXor = 0x09,
        L8 = 0x0A,
        L16 = 0x0B,
        L32 = 0x0C,
        L64 = 0x0D,
        S8 = 0x0E,
        S16 = 0x0F,
        S32 = 0x10,
        S64 = 0x11,
        Beq = 0x14,
        Bne = 0x15,
        Bge = 0x16,
        Ble = 0x17,
        Bgt = 0x18,
        Blt = 0x19,
        Jal = 0x1A,
        Jalr = 0x1B,
    };

    void reset();
    void step(uint32_t);

    [[nodiscard]] static std::string_view version() noexcept;
    [[nodiscard]] std::array<int64_t, 32> get_registers();

    [[nodiscard]] static constexpr std::uint32_t decode_field_1(std::uint32_t instruction) noexcept {
        return (instruction >> 7U) & 0x1FU;
    }

    [[nodiscard]] static constexpr std::uint32_t decode_field_2(std::uint32_t instruction) noexcept {
        return (instruction >> 12U) & 0x1FU;
    }

    [[nodiscard]] static constexpr std::uint32_t decode_field_3(std::uint32_t instruction) noexcept {
        return (instruction >> 17U) & 0x1FU;
    }

    [[nodiscard]] static constexpr std::int32_t decode_imm_15(std::uint32_t instruction) noexcept {
        return static_cast<std::int32_t>(instruction) >> 17;
    }

    [[nodiscard]] static constexpr std::int32_t decode_imm_20(std::uint32_t instruction) noexcept {
        return static_cast<std::int32_t>(instruction) >> 12;
    }

    [[nodiscard]] static constexpr std::uint64_t add_offset(std::uint64_t base, std::int32_t offset) noexcept {
        return base + static_cast<std::uint64_t>(offset);
    }

  private:
    // todo: we actually have 1GiB memory
    static constexpr std::size_t kMemorySize = 64 * 1024;

    [[nodiscard]] static bool is_memory_access_in_bounds(
        const std::array<std::uint8_t, kMemorySize>& memory, std::uint64_t address, std::size_t width
    ) noexcept;
    [[nodiscard]] static std::uint64_t read_little_endian(
        const std::array<std::uint8_t, kMemorySize>& memory, std::uint64_t address, std::size_t width, bool& ok
    ) noexcept;
    [[nodiscard]] static bool write_little_endian(
        std::array<std::uint8_t, kMemorySize>& memory, std::uint64_t address, std::size_t width, std::uint64_t value
    ) noexcept;
    [[nodiscard]] static std::uint64_t signed_divide(std::uint64_t lhs_bits, std::uint64_t rhs_bits, bool& ok) noexcept;

    std::uint64_t pc = 0;
    std::array<int64_t, 32> registers{};
    std::array<std::uint8_t, kMemorySize> memory{};
};

} // namespace zenith::emulator
