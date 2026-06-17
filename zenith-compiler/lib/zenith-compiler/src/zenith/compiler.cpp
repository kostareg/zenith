#include "zenith/compiler.hpp"

#include "zenith/lexer.hpp"
#include "zenith/preprocessor.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace zenith::compiler {
namespace {

constexpr std::int64_t kWordSize = 8;
constexpr std::int64_t kSavedRegisterBytes = 16;
constexpr std::uint64_t kMaxPositiveImmediate = 16383;
constexpr std::uint64_t kConstantChunkBits = 14;
constexpr std::uint64_t kConstantChunkMask = (1ULL << kConstantChunkBits) - 1ULL;
constexpr std::int64_t kInitialStackPointer = 16000;

template <typename... Visitors> struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

template <typename... Visitors> Overloaded(Visitors...) -> Overloaded<Visitors...>;

[[nodiscard]] std::string source_location(SourceLocation location) {
    return "line " + std::to_string(location.line) + ", column " + std::to_string(location.column);
}

[[noreturn]] void fail_at(SourceLocation location, std::string message) {
    throw std::runtime_error(source_location(location) + ": " + std::move(message));
}

[[nodiscard]] std::string require_name(const Declarator& declarator, std::string_view context) {
    if (!declarator.name.has_value()) {
        fail_at(declarator.location, "expected a name for " + std::string(context));
    }

    return *declarator.name;
}

[[nodiscard]] std::int64_t align_to_word(std::int64_t value) noexcept {
    return ((value + kWordSize - 1) / kWordSize) * kWordSize;
}

class Emitter {
  public:
    void line(std::string value) {
        lines.push_back(std::move(value));
    }

    void emit(std::string line) {
        lines.push_back("  " + std::move(line));
    }

    void label(const std::string& name) {
        lines.push_back(name + ":");
    }

    void append(const Emitter& other) {
        lines.insert(lines.end(), other.lines.begin(), other.lines.end());
    }

    [[nodiscard]] std::string str() const {
        std::ostringstream output;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) {
                output << '\n';
            }
            output << lines[i];
        }
        output << '\n';
        return output.str();
    }

  private:
    std::vector<std::string> lines;
};

struct Symbol {
    std::int64_t offset = 0;
    std::size_t slots = 1;
    bool is_array = false;
};

struct FunctionInfo {
    const FunctionDefinition* definition = nullptr;
    std::string label;
};

class CodeGenerator {
  public:
    explicit CodeGenerator(const Ast& ast)
        : ast(ast) {}

    [[nodiscard]] std::string compile() {
        collect_declarations();
        if (function_order.empty()) {
            throw std::runtime_error("cannot compile a program with no function definitions");
        }

        emitter->line(".main");
        emitter->label("start");
        emit_load_constant("sp", static_cast<std::uint64_t>(kInitialStackPointer));
        emitter->emit("jal ra, " + entry_label());
        emitter->label("exit");
        emitter->emit("j exit");

        for (const std::string& name : function_order) {
            compile_function(functions.at(name));
        }

        return output.str();
    }

  private:
    const Ast& ast;
    Emitter output;
    Emitter* emitter = &output;
    std::unordered_map<std::string, FunctionInfo> functions;
    std::vector<std::string> function_order;
    std::unordered_map<std::string, std::int64_t> constants;
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
    std::vector<std::string> break_labels;
    std::vector<std::string> continue_labels;
    std::string current_return_label;
    int next_function_label = 0;
    int next_label = 0;
    std::int64_t next_frame_offset = kSavedRegisterBytes;

    void collect_declarations() {
        for (const ExternalDeclaration& declaration : ast.declarations) {
            std::visit(
                Overloaded{
                    [this](const VariableDeclaration& variable) {
                        register_type_constants(*variable.type);
                        if (!variable.declarators.empty()) {
                            fail_at(variable.location, "global variables are not supported by the compiler yet");
                        }
                    },
                    [this](const FunctionDeclaration& function) {
                        register_function_type_constants(function);
                    },
                    [this](const FunctionDefinition& function) {
                        register_function_type_constants(function.declaration);

                        const std::string name = require_name(function.declaration.declarator, "function");
                        if (functions.contains(name)) {
                            fail_at(function.declaration.location, "duplicate function definition '" + name + "'");
                        }

                        const std::string label = "F" + std::to_string(next_function_label++);
                        functions.emplace(name, FunctionInfo{&function, label});
                        function_order.push_back(name);
                    },
                },
                declaration
            );
        }
    }

    void register_function_type_constants(const FunctionDeclaration& function) {
        register_type_constants(*function.return_type);
        for (const FunctionParameter& parameter : function.parameters) {
            register_type_constants(*parameter.type);
        }
    }

    void register_type_constants(const Type& type) {
        if (!type.enum_specifier.has_value() || !type.enum_specifier->has_definition) {
            return;
        }

        std::int64_t next_value = 0;
        for (const EnumEnumerator& enumerator : type.enum_specifier->enumerators) {
            const std::int64_t value =
                enumerator.value == nullptr ? next_value : require_constant(*enumerator.value, "enum value");
            constants[enumerator.name] = value;
            next_value = value + 1;
        }
    }

    [[nodiscard]] std::string entry_label() const {
        if (const auto main = functions.find("main"); main != functions.end()) {
            return main->second.label;
        }

        return functions.at(function_order.front()).label;
    }

    [[nodiscard]] std::string new_label() {
        return "L" + std::to_string(next_label++);
    }

    void compile_function(const FunctionInfo& function) {
        const FunctionDeclaration& declaration = function.definition->declaration;
        if (declaration.parameters.size() > 8) {
            fail_at(declaration.location, "functions with more than 8 parameters are not supported");
        }

        Emitter body;
        Emitter* previous_emitter = emitter;
        emitter = &body;

        scopes.clear();
        break_labels.clear();
        continue_labels.clear();
        next_frame_offset = kSavedRegisterBytes;
        current_return_label = new_label();

        enter_scope();
        for (std::size_t i = 0; i < declaration.parameters.size(); ++i) {
            const FunctionParameter& parameter = declaration.parameters[i];
            if (!parameter.declarator.name.has_value()) {
                continue;
            }

            if (!parameter.declarator.array_dimensions.empty()) {
                fail_at(parameter.declarator.location, "array parameters are not supported yet");
            }

            const std::string name = *parameter.declarator.name;
            const std::int64_t offset = allocate_slots(1, parameter.declarator.location);
            define_symbol(name, Symbol{offset, 1, false}, parameter.declarator.location);
            emitter->emit("s64 a" + std::to_string(i) + ", s0, " + std::to_string(offset));
        }

        compile_compound_statement(function.definition->body);
        exit_scope();

        const std::int64_t frame_size = align_to_word(next_frame_offset);
        if (frame_size > kInitialStackPointer) {
            fail_at(declaration.location, "function stack frame is too large");
        }

        emitter = previous_emitter;
        emitter->label(function.label);
        emit_prologue(frame_size);
        emitter->append(body);
        emitter->label(current_return_label);
        emit_epilogue(frame_size);
    }

    void emit_prologue(std::int64_t frame_size) {
        emit_load_constant("t6", static_cast<std::uint64_t>(frame_size));
        emitter->emit("sub sp, sp, t6");
        emitter->emit("s64 ra, sp, 0");
        emitter->emit("s64 s0, sp, 8");
        emitter->emit("add s0, sp, zero");
    }

    void emit_epilogue(std::int64_t frame_size) {
        emitter->emit("add sp, s0, zero");
        emitter->emit("l64 s0, sp, 8");
        emitter->emit("l64 ra, sp, 0");
        emit_load_constant("t6", static_cast<std::uint64_t>(frame_size));
        emitter->emit("add sp, sp, t6");
        emitter->emit("jalr zero, ra, 0");
    }

    void enter_scope() {
        scopes.emplace_back();
    }

    void exit_scope() {
        scopes.pop_back();
    }

    void define_symbol(std::string name, Symbol symbol, SourceLocation location) {
        auto& scope = scopes.back();
        if (scope.contains(name)) {
            fail_at(location, "duplicate local name '" + name + "'");
        }

        scope.emplace(std::move(name), symbol);
    }

    [[nodiscard]] const Symbol* find_symbol(const std::string& name) const {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            if (const auto symbol = scope->find(name); symbol != scope->end()) {
                return &symbol->second;
            }
        }

        return nullptr;
    }

    [[nodiscard]] std::int64_t allocate_slots(std::size_t slots, SourceLocation location) {
        if (slots == 0 || slots > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() / kWordSize)) {
            fail_at(location, "invalid storage size");
        }

        const std::int64_t offset = next_frame_offset;
        next_frame_offset += static_cast<std::int64_t>(slots) * kWordSize;
        return offset;
    }

    void compile_statement(const Statement& statement) {
        std::visit(
            Overloaded{
                [](const EmptyStatement&) {},
                [this](const ExpressionStatement& expression) {
                    compile_expression(*expression.expression);
                },
                [this](const DeclarationStatement& declaration) {
                    compile_variable_declaration(declaration.declaration);
                },
                [this](const CompoundStatement& compound) {
                    compile_compound_statement(compound);
                },
                [this](const IfStatement& if_statement) {
                    compile_if_statement(if_statement);
                },
                [this](const WhileStatement& while_statement) {
                    compile_while_statement(while_statement);
                },
                [this](const ForStatement& for_statement) {
                    compile_for_statement(for_statement);
                },
                [this](const BreakStatement&) {
                    if (break_labels.empty()) {
                        throw std::runtime_error("'break' used outside of a loop or switch");
                    }
                    emitter->emit("j " + break_labels.back());
                },
                [this](const ContinueStatement&) {
                    if (continue_labels.empty()) {
                        throw std::runtime_error("'continue' used outside of a loop");
                    }
                    emitter->emit("j " + continue_labels.back());
                },
                [this](const ReturnStatement& return_statement) {
                    compile_return_statement(return_statement);
                },
                [this](const SwitchStatement& switch_statement) {
                    compile_switch_statement(switch_statement);
                },
                [](const CaseStatement&) {
                    throw std::runtime_error("'case' used outside of a supported switch body");
                },
                [](const DefaultStatement&) {
                    throw std::runtime_error("'default' used outside of a supported switch body");
                },
            },
            statement.node
        );
    }

    void compile_compound_statement(const CompoundStatement& compound) {
        enter_scope();
        for (const StatementPtr& statement : compound.statements) {
            compile_statement(*statement);
        }
        exit_scope();
    }

    void compile_if_statement(const IfStatement& if_statement) {
        const std::string else_label = new_label();
        const std::string end_label = new_label();

        compile_expression(*if_statement.condition);
        emitter->emit("beq t0, zero, " + else_label);
        compile_statement(*if_statement.then_branch);
        emitter->emit("j " + end_label);
        emitter->label(else_label);
        if (if_statement.else_branch != nullptr) {
            compile_statement(*if_statement.else_branch);
        }
        emitter->label(end_label);
    }

    void compile_while_statement(const WhileStatement& while_statement) {
        const std::string condition_label = new_label();
        const std::string end_label = new_label();

        emitter->label(condition_label);
        compile_expression(*while_statement.condition);
        emitter->emit("beq t0, zero, " + end_label);

        break_labels.push_back(end_label);
        continue_labels.push_back(condition_label);
        compile_statement(*while_statement.body);
        continue_labels.pop_back();
        break_labels.pop_back();

        emitter->emit("j " + condition_label);
        emitter->label(end_label);
    }

    void compile_for_statement(const ForStatement& for_statement) {
        enter_scope();

        std::visit(
            Overloaded{
                [](const std::monostate&) {},
                [this](const VariableDeclaration& declaration) {
                    compile_variable_declaration(declaration);
                },
                [this](const ExpressionPtr& expression) {
                    compile_expression(*expression);
                },
            },
            for_statement.initializer.value
        );

        const std::string condition_label = new_label();
        const std::string increment_label = new_label();
        const std::string end_label = new_label();

        emitter->label(condition_label);
        if (for_statement.condition != nullptr) {
            compile_expression(*for_statement.condition);
            emitter->emit("beq t0, zero, " + end_label);
        }

        break_labels.push_back(end_label);
        continue_labels.push_back(increment_label);
        compile_statement(*for_statement.body);
        continue_labels.pop_back();
        break_labels.pop_back();

        emitter->label(increment_label);
        if (for_statement.increment != nullptr) {
            compile_expression(*for_statement.increment);
        }
        emitter->emit("j " + condition_label);
        emitter->label(end_label);

        exit_scope();
    }

    void compile_return_statement(const ReturnStatement& return_statement) {
        if (return_statement.value != nullptr) {
            compile_expression(*return_statement.value);
            move_register("a0", "t0");
        }
        emitter->emit("j " + current_return_label);
    }

    struct SwitchArm {
        const Statement* body = nullptr;
        std::string label;
        std::optional<std::int64_t> value;
    };

    void compile_switch_statement(const SwitchStatement& switch_statement) {
        const auto* compound = std::get_if<CompoundStatement>(&switch_statement.body->node);
        if (compound == nullptr) {
            fail_at(switch_statement.body->location, "switch bodies must be compound statements");
        }

        std::vector<SwitchArm> arms;
        std::optional<std::string> default_label;
        for (const StatementPtr& statement : compound->statements) {
            if (const auto* case_statement = std::get_if<CaseStatement>(&statement->node)) {
                arms.push_back(
                    SwitchArm{
                        case_statement->body.get(),
                        new_label(),
                        require_constant(*case_statement->expression, "case label"),
                    }
                );
                continue;
            }

            if (const auto* default_statement = std::get_if<DefaultStatement>(&statement->node)) {
                if (default_label.has_value()) {
                    fail_at(statement->location, "duplicate default label in switch");
                }
                default_label = new_label();
                arms.push_back(SwitchArm{default_statement->body.get(), *default_label, std::nullopt});
                continue;
            }

            fail_at(statement->location, "switch body statements must be under case or default labels");
        }

        const std::string end_label = new_label();
        compile_expression(*switch_statement.expression);
        move_register("t2", "t0");

        for (const SwitchArm& arm : arms) {
            if (!arm.value.has_value()) {
                continue;
            }
            emit_load_constant("t1", static_cast<std::uint64_t>(*arm.value));
            emitter->emit("beq t2, t1, " + arm.label);
        }

        emitter->emit("j " + default_label.value_or(end_label));
        break_labels.push_back(end_label);
        for (const SwitchArm& arm : arms) {
            emitter->label(arm.label);
            compile_statement(*arm.body);
        }
        break_labels.pop_back();
        emitter->label(end_label);
    }

    void compile_variable_declaration(const VariableDeclaration& declaration) {
        if (declaration.is_static) {
            fail_at(declaration.location, "static local variables are not supported yet");
        }

        register_type_constants(*declaration.type);
        if (declaration.declarators.empty()) {
            return;
        }

        if (declaration.type->struct_specifier.has_value()) {
            fail_at(declaration.location, "struct variables are not supported yet");
        }

        for (const VariableDeclarator& variable : declaration.declarators) {
            const std::string name = require_name(variable.declarator, "local variable");
            const std::size_t slots = slots_for_declarator(variable.declarator, variable.initializer.get());
            const std::int64_t offset = allocate_slots(slots, variable.declarator.location);
            const bool is_array = !variable.declarator.array_dimensions.empty();

            define_symbol(name, Symbol{offset, slots, is_array}, variable.declarator.location);
            compile_initializer(variable.initializer.get(), offset, slots, is_array);
        }
    }

    [[nodiscard]] std::size_t slots_for_declarator(const Declarator& declarator, const Initializer* initializer) {
        if (declarator.array_dimensions.empty()) {
            return 1;
        }

        std::size_t slots = 1;
        for (std::size_t i = 0; i < declarator.array_dimensions.size(); ++i) {
            const ArrayDimension& dimension = declarator.array_dimensions[i];
            std::int64_t size = 0;
            if (dimension.size != nullptr) {
                size = require_constant(*dimension.size, "array dimension");
            } else if (i == 0 && declarator.array_dimensions.size() == 1 && initializer != nullptr &&
                       initializer->is_list()) {
                size = static_cast<std::int64_t>(initializer->values.size());
            } else {
                fail_at(declarator.location, "array dimensions must have compile-time sizes");
            }

            if (size <= 0) {
                fail_at(declarator.location, "array dimensions must be positive");
            }
            if (slots > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(size)) {
                fail_at(declarator.location, "array storage is too large");
            }
            slots *= static_cast<std::size_t>(size);
        }

        return slots;
    }

    void compile_initializer(const Initializer* initializer, std::int64_t offset, std::size_t slots, bool is_array) {
        if (initializer == nullptr) {
            return;
        }

        if (!initializer->is_list()) {
            if (is_array) {
                fail_at(initializer->location, "array initializers must use initializer lists");
            }

            compile_expression(*initializer->expression);
            emit_store_to_frame("t0", offset);
            return;
        }

        if (!is_array && initializer->values.size() != 1) {
            fail_at(initializer->location, "scalar initializer lists must contain exactly one value");
        }

        if (initializer->values.size() > slots) {
            fail_at(initializer->location, "too many values in initializer list");
        }

        for (std::size_t i = 0; i < initializer->values.size(); ++i) {
            const Initializer& value = *initializer->values[i];
            if (value.is_list()) {
                fail_at(value.location, "nested initializer lists are not supported yet");
            }

            compile_expression(*value.expression);
            emit_store_to_frame("t0", offset + static_cast<std::int64_t>(i) * kWordSize);
        }

        for (std::size_t i = initializer->values.size(); i < slots; ++i) {
            emit_load_constant("t0", 0);
            emit_store_to_frame("t0", offset + static_cast<std::int64_t>(i) * kWordSize);
        }
    }

    void compile_expression(const Expression& expression) {
        std::visit(
            Overloaded{
                [this](const LiteralExpression& literal) {
                    compile_literal(literal);
                },
                [this](const IdentifierExpression& identifier) {
                    compile_identifier(identifier);
                },
                [this](const UnaryExpression& unary) {
                    compile_unary_expression(unary);
                },
                [this](const BinaryExpression& binary) {
                    compile_binary_expression(binary);
                },
                [this](const AssignmentExpression& assignment) {
                    compile_assignment_expression(assignment);
                },
                [this](const ConditionalExpression& conditional) {
                    compile_conditional_expression(conditional);
                },
                [this](const CallExpression& call) {
                    compile_call_expression(call);
                },
                [this](const IndexExpression& index) {
                    compile_lvalue_address(index);
                    emitter->emit("l64 t0, t0, 0");
                },
                [](const MemberExpression&) {
                    throw std::runtime_error("struct member access is not supported yet");
                },
            },
            expression.node
        );
    }

    void compile_literal(const LiteralExpression& literal) {
        switch (literal.kind) {
        case LiteralKind::Integer:
        case LiteralKind::Character:
            emit_load_constant("t0", literal.integer_value);
            return;
        case LiteralKind::Boolean:
            emit_load_constant("t0", literal.boolean_value ? 1U : 0U);
            return;
        case LiteralKind::String:
            throw std::runtime_error("string literals are not supported yet");
        }

        throw std::runtime_error("unsupported literal");
    }

    void compile_identifier(const IdentifierExpression& identifier) {
        if (const Symbol* symbol = find_symbol(identifier.name)) {
            if (symbol->is_array) {
                emit_frame_address(symbol->offset);
            } else {
                emit_load_from_frame("t0", symbol->offset);
            }
            return;
        }

        if (const auto constant = constants.find(identifier.name); constant != constants.end()) {
            emit_load_constant("t0", static_cast<std::uint64_t>(constant->second));
            return;
        }

        throw std::runtime_error("unknown identifier '" + identifier.name + "'");
    }

    void compile_unary_expression(const UnaryExpression& unary) {
        switch (unary.op) {
        case UnaryOperator::Plus:
            compile_expression(*unary.operand);
            return;
        case UnaryOperator::Minus:
            compile_expression(*unary.operand);
            emitter->emit("sub t0, zero, t0");
            return;
        case UnaryOperator::LogicalNot:
            compile_expression(*unary.operand);
            emit_logical_not();
            return;
        case UnaryOperator::BitwiseNot:
            compile_expression(*unary.operand);
            emitter->emit("not t0, t0");
            return;
        case UnaryOperator::AddressOf:
            compile_lvalue_address(*unary.operand);
            return;
        case UnaryOperator::Dereference:
            compile_expression(*unary.operand);
            emitter->emit("l64 t0, t0, 0");
            return;
        case UnaryOperator::PreIncrement:
            compile_increment(*unary.operand, true, true);
            return;
        case UnaryOperator::PreDecrement:
            compile_increment(*unary.operand, false, true);
            return;
        case UnaryOperator::PostIncrement:
            compile_increment(*unary.operand, true, false);
            return;
        case UnaryOperator::PostDecrement:
            compile_increment(*unary.operand, false, false);
            return;
        }

        throw std::runtime_error("unsupported unary expression");
    }

    void compile_increment(const Expression& target, bool increment, bool result_is_new_value) {
        if (is_array_identifier(target)) {
            fail_at(target.location, "cannot increment an array");
        }

        compile_lvalue_address(target);
        emitter->emit("l64 t1, t0, 0");
        if (increment) {
            emitter->emit("addi t2, t1, 1");
        } else {
            emit_load_constant("t2", 1);
            emitter->emit("sub t2, t1, t2");
        }
        emitter->emit("s64 t2, t0, 0");
        move_register("t0", result_is_new_value ? "t2" : "t1");
    }

    void compile_binary_expression(const BinaryExpression& binary) {
        if (binary.op == BinaryOperator::LogicalAnd) {
            compile_logical_and(binary);
            return;
        }
        if (binary.op == BinaryOperator::LogicalOr) {
            compile_logical_or(binary);
            return;
        }

        compile_expression(*binary.left);
        push_register("t0");
        compile_expression(*binary.right);
        pop_register("t1");
        emit_binary_operation(binary.op, "t1", "t0", "t0");
    }

    void compile_logical_and(const BinaryExpression& binary) {
        const std::string false_label = new_label();
        const std::string end_label = new_label();

        compile_expression(*binary.left);
        emitter->emit("beq t0, zero, " + false_label);
        compile_expression(*binary.right);
        emitter->emit("beq t0, zero, " + false_label);
        emit_load_constant("t0", 1);
        emitter->emit("j " + end_label);
        emitter->label(false_label);
        emit_load_constant("t0", 0);
        emitter->label(end_label);
    }

    void compile_logical_or(const BinaryExpression& binary) {
        const std::string true_label = new_label();
        const std::string end_label = new_label();

        compile_expression(*binary.left);
        emitter->emit("bne t0, zero, " + true_label);
        compile_expression(*binary.right);
        emitter->emit("bne t0, zero, " + true_label);
        emit_load_constant("t0", 0);
        emitter->emit("j " + end_label);
        emitter->label(true_label);
        emit_load_constant("t0", 1);
        emitter->label(end_label);
    }

    void compile_assignment_expression(const AssignmentExpression& assignment) {
        if (is_array_identifier(*assignment.target)) {
            fail_at(assignment.target->location, "cannot assign to an array");
        }

        compile_lvalue_address(*assignment.target);
        push_register("t0");
        compile_expression(*assignment.value);
        pop_register("t1");

        if (assignment.op != AssignmentOperator::Assign) {
            emitter->emit("l64 t2, t1, 0");
            emit_assignment_operation(assignment.op, "t2", "t0", "t0");
        }

        emitter->emit("s64 t0, t1, 0");
    }

    void emit_assignment_operation(
        AssignmentOperator op, std::string_view lhs, std::string_view rhs, std::string_view target
    ) {
        switch (op) {
        case AssignmentOperator::Assign:
            move_register(std::string(target), std::string(rhs));
            return;
        case AssignmentOperator::AddAssign:
            emit_binary_operation(BinaryOperator::Add, lhs, rhs, target);
            return;
        case AssignmentOperator::SubtractAssign:
            emit_binary_operation(BinaryOperator::Subtract, lhs, rhs, target);
            return;
        case AssignmentOperator::MultiplyAssign:
            emit_binary_operation(BinaryOperator::Multiply, lhs, rhs, target);
            return;
        case AssignmentOperator::DivideAssign:
            emit_binary_operation(BinaryOperator::Divide, lhs, rhs, target);
            return;
        case AssignmentOperator::ModuloAssign:
            emit_binary_operation(BinaryOperator::Modulo, lhs, rhs, target);
            return;
        case AssignmentOperator::ShiftLeftAssign:
            emit_binary_operation(BinaryOperator::ShiftLeft, lhs, rhs, target);
            return;
        case AssignmentOperator::ShiftRightAssign:
            emit_binary_operation(BinaryOperator::ShiftRight, lhs, rhs, target);
            return;
        case AssignmentOperator::BitwiseAndAssign:
            emit_binary_operation(BinaryOperator::BitwiseAnd, lhs, rhs, target);
            return;
        case AssignmentOperator::BitwiseXorAssign:
            emit_binary_operation(BinaryOperator::BitwiseXor, lhs, rhs, target);
            return;
        case AssignmentOperator::BitwiseOrAssign:
            emit_binary_operation(BinaryOperator::BitwiseOr, lhs, rhs, target);
            return;
        }

        throw std::runtime_error("unsupported assignment operator");
    }

    void compile_conditional_expression(const ConditionalExpression& conditional) {
        const std::string else_label = new_label();
        const std::string end_label = new_label();

        compile_expression(*conditional.condition);
        emitter->emit("beq t0, zero, " + else_label);
        compile_expression(*conditional.then_branch);
        emitter->emit("j " + end_label);
        emitter->label(else_label);
        compile_expression(*conditional.else_branch);
        emitter->label(end_label);
    }

    void compile_call_expression(const CallExpression& call) {
        const auto* callee = std::get_if<IdentifierExpression>(&call.callee->node);
        if (callee == nullptr) {
            fail_at(call.callee->location, "only direct function calls are supported");
        }

        const auto function = functions.find(callee->name);
        if (function == functions.end()) {
            fail_at(call.callee->location, "unknown function '" + callee->name + "'");
        }
        if (call.arguments.size() > 8) {
            fail_at(call.callee->location, "function calls with more than 8 arguments are not supported");
        }

        for (const ExpressionPtr& argument : call.arguments) {
            compile_expression(*argument);
            push_register("t0");
        }

        for (std::size_t i = call.arguments.size(); i > 0; --i) {
            pop_register("a" + std::to_string(i - 1));
        }

        emitter->emit("jal ra, " + function->second.label);
        move_register("t0", "a0");
    }

    void emit_binary_operation(
        BinaryOperator op, std::string_view lhs, std::string_view rhs, std::string_view target
    ) {
        const std::string lhs_reg(lhs);
        const std::string rhs_reg(rhs);
        const std::string target_reg(target);

        switch (op) {
        case BinaryOperator::Multiply:
            emitter->emit("mul " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::Divide:
            emitter->emit("div " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::Modulo:
            emitter->emit("div t3, " + lhs_reg + ", " + rhs_reg);
            emitter->emit("mul t3, t3, " + rhs_reg);
            emitter->emit("sub " + target_reg + ", " + lhs_reg + ", t3");
            return;
        case BinaryOperator::Add:
            emitter->emit("add " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::Subtract:
            emitter->emit("sub " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::ShiftLeft:
            emitter->emit("sll " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::ShiftRight:
            emitter->emit("sra " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::Less:
            emit_comparison_result("blt", lhs_reg, rhs_reg, target_reg);
            return;
        case BinaryOperator::LessEqual:
            emit_comparison_result("ble", lhs_reg, rhs_reg, target_reg);
            return;
        case BinaryOperator::Greater:
            emit_comparison_result("bgt", lhs_reg, rhs_reg, target_reg);
            return;
        case BinaryOperator::GreaterEqual:
            emit_comparison_result("bge", lhs_reg, rhs_reg, target_reg);
            return;
        case BinaryOperator::Equal:
            emit_comparison_result("beq", lhs_reg, rhs_reg, target_reg);
            return;
        case BinaryOperator::NotEqual:
            emit_comparison_result("bne", lhs_reg, rhs_reg, target_reg);
            return;
        case BinaryOperator::BitwiseAnd:
            emitter->emit("and " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::BitwiseXor:
            emitter->emit("xor " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::BitwiseOr:
            emitter->emit("or " + target_reg + ", " + lhs_reg + ", " + rhs_reg);
            return;
        case BinaryOperator::LogicalAnd:
        case BinaryOperator::LogicalOr:
            throw std::runtime_error("internal compiler error: logical operation reached binary emitter");
        }

        throw std::runtime_error("unsupported binary operator");
    }

    void emit_comparison_result(
        std::string_view branch, const std::string& lhs, const std::string& rhs, const std::string& target
    ) {
        const std::string true_label = new_label();
        const std::string end_label = new_label();

        emitter->emit(std::string(branch) + " " + lhs + ", " + rhs + ", " + true_label);
        emit_load_constant(target, 0);
        emitter->emit("j " + end_label);
        emitter->label(true_label);
        emit_load_constant(target, 1);
        emitter->label(end_label);
    }

    void emit_logical_not() {
        const std::string true_label = new_label();
        const std::string end_label = new_label();

        emitter->emit("beq t0, zero, " + true_label);
        emit_load_constant("t0", 0);
        emitter->emit("j " + end_label);
        emitter->label(true_label);
        emit_load_constant("t0", 1);
        emitter->label(end_label);
    }

    void compile_lvalue_address(const Expression& expression) {
        std::visit(
            Overloaded{
                [this, &expression](const IdentifierExpression& identifier) {
                    const Symbol* symbol = find_symbol(identifier.name);
                    if (symbol == nullptr) {
                        fail_at(expression.location, "unknown assignable identifier '" + identifier.name + "'");
                    }
                    emit_frame_address(symbol->offset);
                },
                [this, &expression](const UnaryExpression& unary) {
                    if (unary.op != UnaryOperator::Dereference) {
                        fail_at(expression.location, "expression is not assignable");
                    }
                    compile_expression(*unary.operand);
                },
                [this](const IndexExpression& index) {
                    compile_index_address(index);
                },
                [&expression](const auto&) {
                    fail_at(expression.location, "expression is not assignable");
                },
            },
            expression.node
        );
    }

    void compile_lvalue_address(const IndexExpression& index) {
        compile_index_address(index);
    }

    void compile_index_address(const IndexExpression& index) {
        if (const auto* identifier = std::get_if<IdentifierExpression>(&index.object->node)) {
            if (const Symbol* symbol = find_symbol(identifier->name); symbol != nullptr && symbol->is_array) {
                emit_frame_address(symbol->offset);
            } else {
                compile_expression(*index.object);
            }
        } else {
            compile_expression(*index.object);
        }

        push_register("t0");
        compile_expression(*index.index);
        emit_load_constant("t6", static_cast<std::uint64_t>(kWordSize));
        emitter->emit("mul t0, t0, t6");
        pop_register("t1");
        emitter->emit("add t0, t1, t0");
    }

    [[nodiscard]] bool is_array_identifier(const Expression& expression) const {
        const auto* identifier = std::get_if<IdentifierExpression>(&expression.node);
        if (identifier == nullptr) {
            return false;
        }

        const Symbol* symbol = find_symbol(identifier->name);
        return symbol != nullptr && symbol->is_array;
    }

    void emit_frame_address(std::int64_t offset) {
        emit_add_immediate("t0", "s0", offset);
    }

    void emit_load_from_frame(std::string_view reg, std::int64_t offset) {
        ensure_frame_offset(offset);
        emitter->emit("l64 " + std::string(reg) + ", s0, " + std::to_string(offset));
    }

    void emit_store_to_frame(std::string_view reg, std::int64_t offset) {
        ensure_frame_offset(offset);
        emitter->emit("s64 " + std::string(reg) + ", s0, " + std::to_string(offset));
    }

    void ensure_frame_offset(std::int64_t offset) {
        if (offset < 0 || static_cast<std::uint64_t>(offset) > kMaxPositiveImmediate) {
            throw std::runtime_error("stack frame offset is out of range");
        }
    }

    void emit_add_immediate(std::string_view destination, std::string_view source, std::int64_t value) {
        const std::string destination_reg(destination);
        const std::string source_reg(source);

        if (value >= 0 && static_cast<std::uint64_t>(value) <= kMaxPositiveImmediate) {
            emitter->emit("addi " + destination_reg + ", " + source_reg + ", " + std::to_string(value));
            return;
        }

        if (value < 0) {
            emit_load_constant("t6", static_cast<std::uint64_t>(-value));
            emitter->emit("sub " + destination_reg + ", " + source_reg + ", t6");
            return;
        }

        emit_load_constant("t6", static_cast<std::uint64_t>(value));
        emitter->emit("add " + destination_reg + ", " + source_reg + ", t6");
    }

    void push_register(std::string_view reg) {
        const std::string reg_name(reg);
        const std::string scratch = reg_name == "t6" ? "t5" : "t6";
        emit_load_constant(scratch, static_cast<std::uint64_t>(kWordSize));
        emitter->emit("sub sp, sp, " + scratch);
        emitter->emit("s64 " + reg_name + ", sp, 0");
    }

    void pop_register(std::string_view reg) {
        const std::string reg_name(reg);
        emitter->emit("l64 " + reg_name + ", sp, 0");
        emitter->emit("addi sp, sp, " + std::to_string(kWordSize));
    }

    void move_register(std::string_view destination, std::string_view source) {
        if (destination == source) {
            return;
        }

        emitter->emit("add " + std::string(destination) + ", " + std::string(source) + ", zero");
    }

    void emit_load_constant(std::string_view reg, std::uint64_t value) {
        const std::string reg_name(reg);
        if (value <= kMaxPositiveImmediate) {
            emitter->emit("addi " + reg_name + ", zero, " + std::to_string(value));
            return;
        }

        std::vector<std::uint64_t> chunks;
        while (value > 0) {
            chunks.push_back(value & kConstantChunkMask);
            value >>= kConstantChunkBits;
        }

        emitter->emit("addi " + reg_name + ", zero, " + std::to_string(chunks.back()));
        const std::string scratch = reg_name == "t6" ? "t5" : "t6";
        for (std::size_t i = chunks.size() - 1; i > 0; --i) {
            emitter->emit("addi " + scratch + ", zero, " + std::to_string(kConstantChunkBits));
            emitter->emit("sll " + reg_name + ", " + reg_name + ", " + scratch);
            if (chunks[i - 1] != 0) {
                emitter->emit("addi " + reg_name + ", " + reg_name + ", " + std::to_string(chunks[i - 1]));
            }
        }
    }

    [[nodiscard]] std::int64_t require_constant(const Expression& expression, std::string_view context) {
        if (const auto value = evaluate_constant(expression)) {
            return *value;
        }

        fail_at(expression.location, "expected a compile-time constant for " + std::string(context));
    }

    [[nodiscard]] std::optional<std::int64_t> evaluate_constant(const Expression& expression) {
        return std::visit(
            Overloaded{
                [](const LiteralExpression& literal) -> std::optional<std::int64_t> {
                    switch (literal.kind) {
                    case LiteralKind::Integer:
                    case LiteralKind::Character:
                        return static_cast<std::int64_t>(literal.integer_value);
                    case LiteralKind::Boolean:
                        return literal.boolean_value ? 1 : 0;
                    case LiteralKind::String:
                        return std::nullopt;
                    }
                    return std::nullopt;
                },
                [this](const IdentifierExpression& identifier) -> std::optional<std::int64_t> {
                    if (const auto constant = constants.find(identifier.name); constant != constants.end()) {
                        return constant->second;
                    }
                    return std::nullopt;
                },
                [this](const UnaryExpression& unary) -> std::optional<std::int64_t> {
                    const auto operand = evaluate_constant(*unary.operand);
                    if (!operand.has_value()) {
                        return std::nullopt;
                    }

                    switch (unary.op) {
                    case UnaryOperator::Plus:
                        return *operand;
                    case UnaryOperator::Minus:
                        return -*operand;
                    case UnaryOperator::LogicalNot:
                        return *operand == 0 ? 1 : 0;
                    case UnaryOperator::BitwiseNot:
                        return ~*operand;
                    case UnaryOperator::AddressOf:
                    case UnaryOperator::Dereference:
                    case UnaryOperator::PreIncrement:
                    case UnaryOperator::PreDecrement:
                    case UnaryOperator::PostIncrement:
                    case UnaryOperator::PostDecrement:
                        return std::nullopt;
                    }
                    return std::nullopt;
                },
                [this](const BinaryExpression& binary) -> std::optional<std::int64_t> {
                    const auto left = evaluate_constant(*binary.left);
                    const auto right = evaluate_constant(*binary.right);
                    if (!left.has_value() || !right.has_value()) {
                        return std::nullopt;
                    }

                    switch (binary.op) {
                    case BinaryOperator::Multiply:
                        return *left * *right;
                    case BinaryOperator::Divide:
                        if (*right == 0) return std::nullopt;
                        return *left / *right;
                    case BinaryOperator::Modulo:
                        if (*right == 0) return std::nullopt;
                        return *left % *right;
                    case BinaryOperator::Add:
                        return *left + *right;
                    case BinaryOperator::Subtract:
                        return *left - *right;
                    case BinaryOperator::ShiftLeft:
                        return *left << *right;
                    case BinaryOperator::ShiftRight:
                        return *left >> *right;
                    case BinaryOperator::Less:
                        return *left < *right ? 1 : 0;
                    case BinaryOperator::LessEqual:
                        return *left <= *right ? 1 : 0;
                    case BinaryOperator::Greater:
                        return *left > *right ? 1 : 0;
                    case BinaryOperator::GreaterEqual:
                        return *left >= *right ? 1 : 0;
                    case BinaryOperator::Equal:
                        return *left == *right ? 1 : 0;
                    case BinaryOperator::NotEqual:
                        return *left != *right ? 1 : 0;
                    case BinaryOperator::BitwiseAnd:
                        return *left & *right;
                    case BinaryOperator::BitwiseXor:
                        return *left ^ *right;
                    case BinaryOperator::BitwiseOr:
                        return *left | *right;
                    case BinaryOperator::LogicalAnd:
                        return (*left != 0 && *right != 0) ? 1 : 0;
                    case BinaryOperator::LogicalOr:
                        return (*left != 0 || *right != 0) ? 1 : 0;
                    }
                    return std::nullopt;
                },
                [this](const ConditionalExpression& conditional) -> std::optional<std::int64_t> {
                    const auto condition = evaluate_constant(*conditional.condition);
                    if (!condition.has_value()) {
                        return std::nullopt;
                    }
                    return evaluate_constant(*(*condition != 0 ? conditional.then_branch : conditional.else_branch));
                },
                [](const AssignmentExpression&) -> std::optional<std::int64_t> {
                    return std::nullopt;
                },
                [](const CallExpression&) -> std::optional<std::int64_t> {
                    return std::nullopt;
                },
                [](const IndexExpression&) -> std::optional<std::int64_t> {
                    return std::nullopt;
                },
                [](const MemberExpression&) -> std::optional<std::int64_t> {
                    return std::nullopt;
                },
            },
            expression.node
        );
    }
};

} // namespace

std::string_view Compiler::version() noexcept {
    return "0.1.0";
}

std::string Compiler::compile(const Ast& ast) const {
    return CodeGenerator(ast).compile();
}

std::string Compiler::compile(std::string source) const {
    std::vector<Token> tokens = Lexer(std::move(source)).lex();
    Ast ast = Parser(preprocess(std::move(tokens))).parse();
    return compile(ast);
}

} // namespace zenith::compiler

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>

#include <string>

EMSCRIPTEN_BINDINGS(zenith_compiler) {
    emscripten::class_<zenith::compiler::Compiler>("Compiler")
        .constructor<>()
        .function(
            "compile",
            emscripten::select_overload<std::string(std::string) const>(&zenith::compiler::Compiler::compile)
        );
}

#endif // __EMSCRIPTEN__
