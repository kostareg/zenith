#include "zenith/compiler.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace zenith::assembler {

enum class InstructionFormat {
    Register,
    Immediate,
    Jump,
    JumpPseudo,
};

struct InstructionInfo {
    std::string_view mnemonic;
    std::uint32_t opcode;
    InstructionFormat format;
};

constexpr std::array<InstructionInfo, 26> kInstructions{{
    {"add", 0x00, InstructionFormat::Register},   {"sub", 0x01, InstructionFormat::Register},
    {"mul", 0x02, InstructionFormat::Register},   {"div", 0x03, InstructionFormat::Register},
    {"addi", 0x04, InstructionFormat::Immediate}, {"muli", 0x05, InstructionFormat::Immediate},
    {"divi", 0x06, InstructionFormat::Immediate}, {"and", 0x07, InstructionFormat::Register},
    {"or", 0x08, InstructionFormat::Register},    {"xor", 0x09, InstructionFormat::Register},
    {"l8", 0x0A, InstructionFormat::Immediate},   {"l16", 0x0B, InstructionFormat::Immediate},
    {"l32", 0x0C, InstructionFormat::Immediate},  {"l64", 0x0D, InstructionFormat::Immediate},
    {"s8", 0x0E, InstructionFormat::Immediate},   {"s16", 0x0F, InstructionFormat::Immediate},
    {"s32", 0x10, InstructionFormat::Immediate},  {"s64", 0x11, InstructionFormat::Immediate},
    {"beq", 0x14, InstructionFormat::Immediate},  {"bne", 0x15, InstructionFormat::Immediate},
    {"bge", 0x16, InstructionFormat::Immediate},  {"ble", 0x17, InstructionFormat::Immediate},
    {"bgt", 0x18, InstructionFormat::Immediate},  {"blt", 0x19, InstructionFormat::Immediate},
    {"jal", 0x1A, InstructionFormat::Jump},       {"jalr", 0x1B, InstructionFormat::Immediate},
}};

constexpr InstructionInfo kJumpPseudo{
    "j",
    0x1A,
    InstructionFormat::JumpPseudo,
};

using Labels = std::unordered_map<std::string, std::int32_t>;

[[nodiscard]] const InstructionInfo* find_instruction(std::string_view mnemonic) {
    if (mnemonic == kJumpPseudo.mnemonic) return &kJumpPseudo;

    for (const InstructionInfo& instruction : kInstructions) {
        if (instruction.mnemonic == mnemonic) return &instruction;
    }

    return nullptr;
}

[[nodiscard]] bool is_digits(std::string_view value) {
    if (value.empty()) return false;

    for (const char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    }

    return true;
}

[[nodiscard]] std::optional<std::uint32_t> parse_numbered_register(std::string_view name) {
    if (name.size() < 2) return std::nullopt;
    if (name[0] != 'r' && name[0] != 'z') return std::nullopt;

    const std::string_view number = name.substr(1);
    if (!is_digits(number)) return std::nullopt;

    std::uint32_t value = 0;
    for (const char ch : number) {
        value = (value * 10U) + static_cast<std::uint32_t>(ch - '0');
    }

    if (value > 31U) return std::nullopt;
    return value;
}

[[nodiscard]] std::optional<std::uint32_t> parse_abi_register(std::string_view name) {
    if (name == "zero") return 0U;
    if (name == "ra") return 1U;
    if (name == "sp") return 2U;

    if (name.size() == 2 && name[0] == 'a' && name[1] >= '0' && name[1] <= '7') {
        return 3U + static_cast<std::uint32_t>(name[1] - '0');
    }

    if (name.size() == 2 && name[0] == 't' && name[1] >= '0' && name[1] <= '7') {
        return 11U + static_cast<std::uint32_t>(name[1] - '0');
    }

    if (name.size() == 2 && name[0] == 's' && name[1] >= '0' && name[1] <= '7') {
        return 19U + static_cast<std::uint32_t>(name[1] - '0');
    }

    if (name.size() == 2 && name[0] == 'k' && name[1] >= '0' && name[1] <= '4') {
        return 27U + static_cast<std::uint32_t>(name[1] - '0');
    }

    return std::nullopt;
}

[[nodiscard]] std::uint32_t register_operand(const Operand& operand, std::string_view mnemonic) {
    if (operand.kind != OperandKind::Identifier) {
        throw std::runtime_error("expected register operand in instruction '" + std::string(mnemonic) + "'");
    }

    if (const auto numbered_register = parse_numbered_register(operand.ident)) return *numbered_register;
    if (const auto abi_register = parse_abi_register(operand.ident)) return *abi_register;

    throw std::runtime_error("unknown register '" + operand.ident + "' in instruction '" + std::string(mnemonic) + "'");
}

[[nodiscard]] bool fits_signed_bits(std::int32_t value, std::uint32_t bits) {
    const std::int32_t min = -(1 << (bits - 1U));
    const std::int32_t max = (1 << (bits - 1U)) - 1;
    return value >= min && value <= max;
}

[[nodiscard]] std::int32_t resolve_immediate(
    const Operand& operand, const Labels& labels, std::int32_t pc, std::string_view mnemonic, bool allow_labels
) {
    if (operand.kind == OperandKind::Number) return operand.number;

    if (!allow_labels) {
        throw std::runtime_error("expected numeric immediate in instruction '" + std::string(mnemonic) + "'");
    }

    const auto label = labels.find(operand.ident);
    if (label == labels.end()) {
        throw std::runtime_error(
            "unknown label '" + operand.ident + "' in instruction '" + std::string(mnemonic) + "'"
        );
    }

    return label->second - pc;
}

[[nodiscard]] std::uint32_t
encode_register(std::uint32_t opcode, std::uint32_t field1, std::uint32_t field2, std::uint32_t field3) {
    return (opcode & 0x7FU) | (field1 << 7U) | (field2 << 12U) | (field3 << 17U);
}

[[nodiscard]] std::uint32_t
encode_immediate(std::uint32_t opcode, std::uint32_t field1, std::uint32_t field2, std::int32_t immediate) {
    if (!fits_signed_bits(immediate, 15U)) {
        throw std::runtime_error("15-bit immediate out of range");
    }

    return (opcode & 0x7FU) | (field1 << 7U) | (field2 << 12U) |
           ((static_cast<std::uint32_t>(immediate) & 0x7FFFU) << 17U);
}

[[nodiscard]] std::uint32_t encode_jump(std::uint32_t opcode, std::uint32_t rd, std::int32_t offset) {
    if (!fits_signed_bits(offset, 20U)) {
        throw std::runtime_error("20-bit jump offset out of range");
    }

    return (opcode & 0x7FU) | (rd << 7U) | ((static_cast<std::uint32_t>(offset) & 0xFFFFFU) << 12U);
}

[[nodiscard]] bool accepts_label_offset(std::string_view mnemonic) {
    return mnemonic == "j" || mnemonic == "jal" || mnemonic == "beq" || mnemonic == "bne" || mnemonic == "bge" ||
           mnemonic == "ble" || mnemonic == "bgt" || mnemonic == "blt";
}

[[nodiscard]] const Section& main_section(const Ast& ast) {
    const Section* main = nullptr;

    for (const Section& section : ast.sections) {
        if (section.name != "main") continue;

        if (main != nullptr) throw std::runtime_error("multiple .main sections");
        main = &section;
    }

    if (main == nullptr) throw std::runtime_error("missing .main section");
    return *main;
}

[[nodiscard]] const Instruction& instruction_from_statement(const Statement& statement) {
    if (!statement.operation.has_value()) {
        throw std::runtime_error("main section contains a label without an instruction");
    }

    if (const auto* directive = std::get_if<Directive>(&*statement.operation)) {
        throw std::runtime_error("main section cannot contain directive '." + directive->name + "'");
    }

    return std::get<Instruction>(*statement.operation);
}

[[nodiscard]] Labels collect_labels(const Section& section) {
    Labels labels;
    std::int32_t pc = 0;

    for (const Statement& statement : section.statements) {
        if (statement.label.has_value()) {
            const auto [_, inserted] = labels.emplace(*statement.label, pc);
            if (!inserted) throw std::runtime_error("duplicate label '" + *statement.label + "'");
        }

        if (statement.operation.has_value()) {
            if (std::holds_alternative<Directive>(*statement.operation)) {
                const auto& directive = std::get<Directive>(*statement.operation);
                throw std::runtime_error("main section cannot contain directive '." + directive.name + "'");
            }

            pc += 4;
        }
    }

    return labels;
}

[[nodiscard]] std::uint32_t compile_instruction(const Instruction& instruction, const Labels& labels, std::int32_t pc) {
    const InstructionInfo* info = find_instruction(instruction.mnemonic);
    if (info == nullptr) throw std::runtime_error("unknown instruction '" + instruction.mnemonic + "'");

    switch (info->format) {
    case InstructionFormat::Register: {
        if (instruction.operands.size() != 3) {
            throw std::runtime_error("instruction '" + instruction.mnemonic + "' expects 3 register operands");
        }

        return encode_register(
            info->opcode,
            register_operand(instruction.operands[0], instruction.mnemonic),
            register_operand(instruction.operands[1], instruction.mnemonic),
            register_operand(instruction.operands[2], instruction.mnemonic)
        );
    }
    case InstructionFormat::Immediate: {
        if (instruction.operands.size() != 3) {
            throw std::runtime_error("instruction '" + instruction.mnemonic + "' expects 3 operands");
        }

        const std::int32_t immediate = resolve_immediate(
            instruction.operands[2], labels, pc, instruction.mnemonic, accepts_label_offset(instruction.mnemonic)
        );

        return encode_immediate(
            info->opcode,
            register_operand(instruction.operands[0], instruction.mnemonic),
            register_operand(instruction.operands[1], instruction.mnemonic),
            immediate
        );
    }
    case InstructionFormat::Jump: {
        if (instruction.operands.size() != 2) {
            throw std::runtime_error("instruction '" + instruction.mnemonic + "' expects rd and offset operands");
        }

        return encode_jump(
            info->opcode,
            register_operand(instruction.operands[0], instruction.mnemonic),
            resolve_immediate(instruction.operands[1], labels, pc, instruction.mnemonic, true)
        );
    }
    case InstructionFormat::JumpPseudo: {
        if (instruction.operands.size() != 1) {
            throw std::runtime_error("instruction 'j' expects one offset operand");
        }

        return encode_jump(info->opcode, 0U, resolve_immediate(instruction.operands[0], labels, pc, "j", true));
    }
    }

    throw std::runtime_error("unsupported instruction '" + instruction.mnemonic + "'");
}

std::vector<std::uint32_t> Compiler::compile() const {
    const Section& main = main_section(ast);
    const Labels labels = collect_labels(main);

    std::vector<std::uint32_t> machine_code;
    machine_code.reserve(main.statements.size());

    std::int32_t pc = 0;
    for (const Statement& statement : main.statements) {
        if (!statement.operation.has_value()) continue;

        const Instruction& instruction = instruction_from_statement(statement);
        machine_code.push_back(compile_instruction(instruction, labels, pc));
        pc += 4;
    }

    return machine_code;
}

} // namespace zenith::assembler
