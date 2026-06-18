#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace zenith::emulator {

class Emulator {
  public:
    Emulator() = default;

    static constexpr std::size_t kFramebufferWidth = 1920;
    static constexpr std::size_t kFramebufferHeight = 1080;
    static constexpr std::size_t kFramebufferBytesPerPixel = 3;
    static constexpr std::size_t kFramebufferSize = kFramebufferWidth * kFramebufferHeight * kFramebufferBytesPerPixel;

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
        BitNot = 0x1C,
        ShiftLeftLogical = 0x1D,
        ShiftRightLogical = 0x1E,
        ShiftLeftArithmetic = 0x1F,
        ShiftRightArithmetic = 0x20,
    };

    void reset();
    [[nodiscard]] bool load_data(const std::vector<std::uint8_t>& data) noexcept;
    [[nodiscard]] bool push_key_event(std::uint16_t key_code, bool pressed, bool repeat = false) noexcept;
    [[nodiscard]] bool push_ps2_scan_code(std::uint8_t scan_code) noexcept;
    void step(uint32_t);

    [[nodiscard]] static std::string_view version() noexcept;
    [[nodiscard]] std::array<int64_t, 32> get_registers();
    [[nodiscard]] const std::array<std::uint8_t, kFramebufferSize>& get_framebuffer() const noexcept;

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
    static constexpr std::size_t kInitialStackPointer = 16000;
    static constexpr std::uint64_t kFramebufferControlBase = 0xFFFFFFFFFFA01400ULL;
    static constexpr std::uint64_t kFramebufferResolutionAddress = kFramebufferControlBase;
    static constexpr std::uint64_t kFramebufferStatusControlAddress = kFramebufferControlBase + 4;
    static constexpr std::uint64_t kFramebufferPixelBase = 0xFFFFFFFFFFA11400ULL;
    static constexpr std::uint32_t kFramebufferEnableBit = 1U << 1U;
    static constexpr std::uint64_t kKeyboardControlBase = 0xFFFFFFFFFFA00000ULL;
    static constexpr std::uint64_t kKeyboardStatusControlAddress = kKeyboardControlBase;
    static constexpr std::uint64_t kKeyboardKeyEventAddress = kKeyboardControlBase + 4;
    static constexpr std::uint64_t kKeyboardFifoCountAddress = kKeyboardControlBase + 8;
    static constexpr std::size_t kKeyboardFifoDepth = 16;
    static constexpr std::uint32_t kKeyboardReadyBit = 1U << 0U;
    static constexpr std::uint32_t kKeyboardFullBit = 1U << 1U;
    static constexpr std::uint32_t kKeyboardOverflowBit = 1U << 2U;
    static constexpr std::uint32_t kKeyboardEnableBit = 1U << 3U;
    static constexpr std::uint32_t kKeyboardClearFifoBit = 1U << 4U;
    static constexpr std::uint32_t kKeyboardPressedBit = 1U << 16U;
    static constexpr std::uint32_t kKeyboardRepeatBit = 1U << 17U;
    static constexpr std::uint16_t kKeyboardHardwarePlaceholderKeyCode = 5;

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
    [[nodiscard]] bool read_memory(std::uint64_t address, std::size_t width, std::uint64_t& value) noexcept;
    [[nodiscard]] bool write_memory(std::uint64_t address, std::size_t width, std::uint64_t value) noexcept;
    [[nodiscard]] bool read_keyboard_register(std::uint64_t address, std::size_t width, std::uint64_t& value) noexcept;
    [[nodiscard]] bool write_keyboard_register(std::uint64_t address, std::size_t width, std::uint64_t value) noexcept;
    [[nodiscard]] std::uint32_t keyboard_status() const noexcept;
    [[nodiscard]] std::uint32_t pop_key_event() noexcept;

    std::uint64_t pc = 0;
    std::array<int64_t, 32> registers{};
    std::array<std::uint8_t, kMemorySize> memory{};
    std::array<std::uint8_t, kFramebufferSize> framebuffer{};
    std::array<std::uint32_t, kKeyboardFifoDepth> keyboard_fifo{};
    std::size_t keyboard_fifo_head = 0;
    std::size_t keyboard_fifo_count = 0;
    bool framebuffer_enabled = false;
    bool keyboard_enabled = false;
    bool keyboard_overflow = false;
};

} // namespace zenith::emulator
