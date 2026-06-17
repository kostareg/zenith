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

struct StorageInfo {
    std::int64_t size = 8;
    bool is_unsigned = false;
};

constexpr std::int64_t kWordSize = 8;
constexpr std::int64_t kSavedRegisterBytes = 16;
constexpr std::uint64_t kMaxPositiveImmediate = 16383;
constexpr std::uint64_t kConstantChunkBits = 14;
constexpr std::uint64_t kConstantChunkMask = (1ULL << kConstantChunkBits) - 1ULL;
constexpr std::int64_t kInitialStackPointer = 16000;

[[nodiscard]] bool has_primitive(const Type& type, PrimitiveType primitive) noexcept {
    for (const PrimitiveType specifier : type.primitive_specifiers) {
        if (specifier == primitive) return true;
    }
    return false;
}

[[nodiscard]] StorageInfo storage_for_type(const Type& type, const Declarator& declarator) noexcept {
    if (declarator.pointer_depth > 0) {
        return StorageInfo{kWordSize, true};
    }

    if (has_primitive(type, PrimitiveType::Bool)) return StorageInfo{1, true};
    if (has_primitive(type, PrimitiveType::Int8)) return StorageInfo{1, false};
    if (has_primitive(type, PrimitiveType::UInt8)) return StorageInfo{1, true};
    if (has_primitive(type, PrimitiveType::Int16)) return StorageInfo{2, false};
    if (has_primitive(type, PrimitiveType::UInt16)) return StorageInfo{2, true};
    if (has_primitive(type, PrimitiveType::Int32)) return StorageInfo{4, false};
    if (has_primitive(type, PrimitiveType::UInt32)) return StorageInfo{4, true};
    if (has_primitive(type, PrimitiveType::Char)) return StorageInfo{1, has_primitive(type, PrimitiveType::Unsigned)};

    return StorageInfo{kWordSize, has_primitive(type, PrimitiveType::Unsigned) || has_primitive(type, PrimitiveType::UInt64)};
}

[[nodiscard]] StorageInfo storage_for_indexed_type(const Type& type, const Declarator& declarator) noexcept {
    if (declarator.pointer_depth == 0) {
        return storage_for_type(type, declarator);
    }

    if (declarator.pointer_depth > 1) {
        return StorageInfo{kWordSize, true};
    }

    Declarator indexed;
    indexed.pointer_depth = 0;
    return storage_for_type(type, indexed);
}

[[nodiscard]] std::uint64_t mask_for_size(std::int64_t size) noexcept {
    if (size >= kWordSize) return std::numeric_limits<std::uint64_t>::max();
    return (1ULL << (static_cast<unsigned>(size) * 8U)) - 1ULL;
}

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
    std::size_t elements = 1;
    std::int64_t element_size = 8;
    bool is_unsigned = false;
    bool is_array = false;
    std::int64_t indexed_element_size = 8;
    bool indexed_is_unsigned = false;
};

struct GlobalSymbol {
    std::int64_t address = 0;
    std::size_t elements = 1;
    std::int64_t element_size = 8;
    bool is_unsigned = false;
    bool is_array = false;
    std::string label;
    std::int64_t indexed_element_size = 8;
    bool indexed_is_unsigned = false;
};

struct GlobalDefinition {
    std::string name;
    GlobalSymbol symbol;
    std::vector<std::int64_t> values;
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

        emit_data_section();

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
    std::unordered_map<std::string, GlobalSymbol> globals;
    std::unordered_map<std::string, GlobalSymbol> string_literals;
    std::vector<GlobalDefinition> global_order;
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
    std::vector<std::string> break_labels;
    std::vector<std::string> continue_labels;
    std::string current_return_label;
    int next_function_label = 0;
    int next_global_label = 0;
    int next_string_label = 0;
    int next_label = 0;
    std::int64_t next_global_address = 0;
    std::int64_t next_frame_offset = kSavedRegisterBytes;

    void collect_declarations() {
        for (const ExternalDeclaration& declaration : ast.declarations) {
            std::visit(
                Overloaded{
                    [this](const VariableDeclaration& variable) {
                        register_type_constants(*variable.type);
                        collect_global_declaration(variable);
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
                        if (globals.contains(name)) {
                            fail_at(function.declaration.location, "function name '" + name + "' conflicts with a global variable");
                        }

                        const std::string label = "F" + std::to_string(next_function_label++);
                        functions.emplace(name, FunctionInfo{&function, label});
                        function_order.push_back(name);
                        collect_string_literals(function.body);
                    },
                },
                declaration
            );
        }

        if (next_global_address > kInitialStackPointer) {
            throw std::runtime_error("global data section overlaps the initial stack");
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
            if (globals.contains(enumerator.name) || functions.contains(enumerator.name)) {
                fail_at(enumerator.location, "enum constant '" + enumerator.name + "' conflicts with an existing name");
            }
            constants[enumerator.name] = value;
            next_value = value + 1;
        }
    }

    void collect_global_declaration(const VariableDeclaration& declaration) {
        if (declaration.is_static) {
            fail_at(declaration.location, "static global variables are not supported");
        }

        if (declaration.declarators.empty()) {
            return;
        }

        if (declaration.type->struct_specifier.has_value()) {
            fail_at(declaration.location, "struct variables are not supported yet");
        }

        for (const VariableDeclarator& variable : declaration.declarators) {
            const std::string name = require_name(variable.declarator, "global variable");
            if (globals.contains(name)) {
                fail_at(variable.declarator.location, "duplicate global name '" + name + "'");
            }
            if (functions.contains(name)) {
                fail_at(variable.declarator.location, "global variable '" + name + "' conflicts with a function");
            }
            if (constants.contains(name)) {
                fail_at(variable.declarator.location, "global variable '" + name + "' conflicts with an enum constant");
            }

            const StorageInfo storage = storage_for_type(*declaration.type, variable.declarator);
            const StorageInfo indexed_storage = storage_for_indexed_type(*declaration.type, variable.declarator);
            const std::size_t elements = elements_for_declarator(variable.declarator, variable.initializer.get());
            const bool is_array = !variable.declarator.array_dimensions.empty();
            const std::int64_t address = allocate_global_storage(elements, storage.size, variable.declarator.location);
            const std::string label = "G" + std::to_string(next_global_label++);
            const GlobalSymbol symbol{
                address,
                elements,
                storage.size,
                storage.is_unsigned,
                is_array,
                label,
                indexed_storage.size,
                indexed_storage.is_unsigned,
            };
            std::vector<std::int64_t> values =
                global_initializer_values(variable.initializer.get(), elements, storage, is_array);

            globals.emplace(name, symbol);
            global_order.push_back(GlobalDefinition{name, symbol, std::move(values)});
        }
    }

    [[nodiscard]] std::int64_t
    allocate_global_storage(std::size_t elements, std::int64_t element_size, SourceLocation location) {
        if (elements == 0 || element_size <= 0 ||
            elements > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() / element_size)) {
            fail_at(location, "invalid global storage size");
        }
        const std::int64_t address = next_global_address;
        next_global_address += static_cast<std::int64_t>(elements) * element_size;
        return address;
    }

    [[nodiscard]] std::vector<std::int64_t>
    global_initializer_values(const Initializer* initializer, std::size_t slots, StorageInfo storage, bool is_array) {
        std::vector<std::int64_t> values(slots, 0);
        if (initializer == nullptr) {
            return values;
        }

        if (!initializer->is_list()) {
            if (is_array && is_string_literal_initializer(*initializer)) {
                fill_string_initializer_values(*initializer->expression, values, storage);
                return values;
            }
            if (is_array) {
                fail_at(initializer->location, "array initializers must use initializer lists");
            }
            values[0] = require_initializer_constant(*initializer->expression, "global initializer");
            return values;
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
            values[i] = require_initializer_constant(*value.expression, "global initializer");
        }

        return values;
    }

    [[nodiscard]] const LiteralExpression* string_literal_expression(const Expression& expression) const noexcept {
        const auto* literal = std::get_if<LiteralExpression>(&expression.node);
        if (literal == nullptr || literal->kind != LiteralKind::String) {
            return nullptr;
        }
        return literal;
    }

    [[nodiscard]] bool is_string_literal_initializer(const Initializer& initializer) const noexcept {
        return initializer.expression != nullptr && string_literal_expression(*initializer.expression) != nullptr;
    }

    [[nodiscard]] std::size_t string_initializer_size(const Expression& expression) const {
        const LiteralExpression* literal = string_literal_expression(expression);
        if (literal == nullptr) {
            throw std::runtime_error("internal compiler error: expected string literal initializer");
        }
        return literal->text.size() + 1;
    }

    [[nodiscard]] std::int64_t require_initializer_constant(const Expression& expression, std::string_view context) {
        if (const LiteralExpression* literal = string_literal_expression(expression)) {
            return intern_string_literal(literal->text, expression.location).address;
        }
        return require_constant(expression, context);
    }

    void fill_string_initializer_values(const Expression& expression, std::vector<std::int64_t>& values, StorageInfo storage) {
        if (storage.size != 1) {
            fail_at(expression.location, "string literals can only initialize byte-sized arrays");
        }

        const LiteralExpression* literal = string_literal_expression(expression);
        if (literal == nullptr) {
            throw std::runtime_error("internal compiler error: expected string literal initializer");
        }
        if (literal->text.size() + 1 > values.size()) {
            fail_at(expression.location, "string literal is too large for array initializer");
        }

        for (std::size_t i = 0; i < literal->text.size(); ++i) {
            values[i] = static_cast<unsigned char>(literal->text[i]);
        }
        values[literal->text.size()] = 0;
    }

    void compile_string_initializer(
        const Expression& expression, std::int64_t offset, std::size_t elements, StorageInfo storage
    ) {
        std::vector<std::int64_t> values(elements, 0);
        fill_string_initializer_values(expression, values, storage);
        for (std::size_t i = 0; i < values.size(); ++i) {
            emit_load_constant("t0", static_cast<std::uint64_t>(values[i]));
            emit_store_to_frame("t0", offset + static_cast<std::int64_t>(i) * storage.size, storage.size);
        }
    }

    const GlobalSymbol& intern_string_literal(const std::string& text, SourceLocation location) {
        if (const auto existing = string_literals.find(text); existing != string_literals.end()) {
            return existing->second;
        }

        const std::size_t elements = text.size() + 1;
        const std::int64_t address = allocate_global_storage(elements, 1, location);
        const std::string label = "S" + std::to_string(next_string_label++);
        const GlobalSymbol symbol{address, elements, 1, true, true, label, 1, true};

        std::vector<std::int64_t> values;
        values.reserve(elements);
        for (const char ch : text) {
            values.push_back(static_cast<unsigned char>(ch));
        }
        values.push_back(0);

        auto [entry, _] = string_literals.emplace(text, symbol);
        global_order.push_back(GlobalDefinition{"", symbol, std::move(values)});
        return entry->second;
    }

    [[nodiscard]] const GlobalSymbol& string_literal_symbol(const std::string& text) const {
        const auto literal = string_literals.find(text);
        if (literal == string_literals.end()) {
            throw std::runtime_error("internal compiler error: missing string literal data");
        }
        return literal->second;
    }

    void collect_string_literals(const Initializer* initializer) {
        if (initializer == nullptr) {
            return;
        }

        if (!initializer->is_list()) {
            collect_string_literals(*initializer->expression);
            return;
        }

        for (const InitializerPtr& value : initializer->values) {
            collect_string_literals(value.get());
        }
    }

    void collect_string_literals(const Statement& statement) {
        std::visit(
            Overloaded{
                [](const EmptyStatement&) {},
                [this](const ExpressionStatement& expression) {
                    collect_string_literals(*expression.expression);
                },
                [this](const DeclarationStatement& declaration) {
                    for (const VariableDeclarator& variable : declaration.declaration.declarators) {
                        collect_string_literals(variable.initializer.get());
                    }
                },
                [this](const CompoundStatement& compound) {
                    collect_string_literals(compound);
                },
                [this](const IfStatement& if_statement) {
                    collect_string_literals(*if_statement.condition);
                    collect_string_literals(*if_statement.then_branch);
                    if (if_statement.else_branch != nullptr) {
                        collect_string_literals(*if_statement.else_branch);
                    }
                },
                [this](const WhileStatement& while_statement) {
                    collect_string_literals(*while_statement.condition);
                    collect_string_literals(*while_statement.body);
                },
                [this](const ForStatement& for_statement) {
                    std::visit(
                        Overloaded{
                            [](const std::monostate&) {},
                            [this](const VariableDeclaration& declaration) {
                                for (const VariableDeclarator& variable : declaration.declarators) {
                                    collect_string_literals(variable.initializer.get());
                                }
                            },
                            [this](const ExpressionPtr& expression) {
                                collect_string_literals(*expression);
                            },
                        },
                        for_statement.initializer.value
                    );
                    if (for_statement.condition != nullptr) {
                        collect_string_literals(*for_statement.condition);
                    }
                    if (for_statement.increment != nullptr) {
                        collect_string_literals(*for_statement.increment);
                    }
                    collect_string_literals(*for_statement.body);
                },
                [](const BreakStatement&) {},
                [](const ContinueStatement&) {},
                [this](const ReturnStatement& return_statement) {
                    if (return_statement.value != nullptr) {
                        collect_string_literals(*return_statement.value);
                    }
                },
                [this](const SwitchStatement& switch_statement) {
                    collect_string_literals(*switch_statement.expression);
                    collect_string_literals(*switch_statement.body);
                },
                [this](const CaseStatement& case_statement) {
                    collect_string_literals(*case_statement.expression);
                    collect_string_literals(*case_statement.body);
                },
                [this](const DefaultStatement& default_statement) {
                    collect_string_literals(*default_statement.body);
                },
            },
            statement.node
        );
    }

    void collect_string_literals(const CompoundStatement& compound) {
        for (const StatementPtr& statement : compound.statements) {
            collect_string_literals(*statement);
        }
    }

    void collect_string_literals(const Expression& expression) {
        std::visit(
            Overloaded{
                [this, &expression](const LiteralExpression& literal) {
                    if (literal.kind == LiteralKind::String) {
                        intern_string_literal(literal.text, expression.location);
                    }
                },
                [](const IdentifierExpression&) {},
                [this](const UnaryExpression& unary) {
                    collect_string_literals(*unary.operand);
                },
                [this](const BinaryExpression& binary) {
                    collect_string_literals(*binary.left);
                    collect_string_literals(*binary.right);
                },
                [this](const AssignmentExpression& assignment) {
                    collect_string_literals(*assignment.target);
                    collect_string_literals(*assignment.value);
                },
                [this](const ConditionalExpression& conditional) {
                    collect_string_literals(*conditional.condition);
                    collect_string_literals(*conditional.then_branch);
                    collect_string_literals(*conditional.else_branch);
                },
                [this](const CallExpression& call) {
                    collect_string_literals(*call.callee);
                    for (const ExpressionPtr& argument : call.arguments) {
                        collect_string_literals(*argument);
                    }
                },
                [this](const IndexExpression& index) {
                    collect_string_literals(*index.object);
                    collect_string_literals(*index.index);
                },
                [this](const MemberExpression& member) {
                    collect_string_literals(*member.object);
                },
            },
            expression.node
        );
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

    void emit_data_section() {
        if (global_order.empty()) {
            return;
        }

        emitter->line(".data");
        for (const GlobalDefinition& global : global_order) {
            emitter->label(global.symbol.label);
            for (const std::int64_t value : global.values) {
                emit_data_value(value, global.symbol.element_size);
            }
        }
    }

    void emit_data_value(std::int64_t value, std::int64_t size) {
        if (size == kWordSize) {
            emitter->emit(".quad " + std::to_string(value));
            return;
        }

        std::ostringstream line;
        line << ".byte ";
        const std::uint64_t bits = static_cast<std::uint64_t>(value) & mask_for_size(size);
        for (std::int64_t i = 0; i < size; ++i) {
            if (i > 0) line << ", ";
            line << ((bits >> (static_cast<unsigned>(i) * 8U)) & 0xFFU);
        }
        emitter->emit(line.str());
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
            const std::int64_t offset = allocate_storage(1, kWordSize, parameter.declarator.location);
            const StorageInfo indexed_storage = storage_for_indexed_type(*parameter.type, parameter.declarator);
            define_symbol(
                name,
                Symbol{offset, 1, kWordSize, false, false, indexed_storage.size, indexed_storage.is_unsigned},
                parameter.declarator.location
            );
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

    [[nodiscard]] std::int64_t allocate_storage(std::size_t elements, std::int64_t element_size, SourceLocation location) {
        if (elements == 0 || element_size <= 0 ||
            elements > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() / element_size)) {
            fail_at(location, "invalid storage size");
        }

        const std::int64_t offset = next_frame_offset;
        next_frame_offset += static_cast<std::int64_t>(elements) * element_size;
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
            const StorageInfo storage = storage_for_type(*declaration.type, variable.declarator);
            const StorageInfo indexed_storage = storage_for_indexed_type(*declaration.type, variable.declarator);
            const std::size_t elements = elements_for_declarator(variable.declarator, variable.initializer.get());
            const std::int64_t offset = allocate_storage(elements, storage.size, variable.declarator.location);
            const bool is_array = !variable.declarator.array_dimensions.empty();

            define_symbol(
                name,
                Symbol{
                    offset,
                    elements,
                    storage.size,
                    storage.is_unsigned,
                    is_array,
                    indexed_storage.size,
                    indexed_storage.is_unsigned,
                },
                variable.declarator.location
            );
            compile_initializer(variable.initializer.get(), offset, elements, storage, is_array);
        }
    }

    [[nodiscard]] std::size_t elements_for_declarator(const Declarator& declarator, const Initializer* initializer) {
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
            } else if (i == 0 && declarator.array_dimensions.size() == 1 && initializer != nullptr &&
                       is_string_literal_initializer(*initializer)) {
                size = static_cast<std::int64_t>(string_initializer_size(*initializer->expression));
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

    void compile_initializer(
        const Initializer* initializer, std::int64_t offset, std::size_t elements, StorageInfo storage, bool is_array
    ) {
        if (initializer == nullptr) {
            return;
        }

        if (!initializer->is_list()) {
            if (is_array && is_string_literal_initializer(*initializer)) {
                compile_string_initializer(*initializer->expression, offset, elements, storage);
                return;
            }
            if (is_array) {
                fail_at(initializer->location, "array initializers must use initializer lists");
            }

            compile_expression(*initializer->expression);
            emit_store_to_frame("t0", offset, storage.size);
            return;
        }

        if (!is_array && initializer->values.size() != 1) {
            fail_at(initializer->location, "scalar initializer lists must contain exactly one value");
        }

        if (initializer->values.size() > elements) {
            fail_at(initializer->location, "too many values in initializer list");
        }

        for (std::size_t i = 0; i < initializer->values.size(); ++i) {
            const Initializer& value = *initializer->values[i];
            if (value.is_list()) {
                fail_at(value.location, "nested initializer lists are not supported yet");
            }

            compile_expression(*value.expression);
            emit_store_to_frame("t0", offset + static_cast<std::int64_t>(i) * storage.size, storage.size);
        }

        for (std::size_t i = initializer->values.size(); i < elements; ++i) {
            emit_load_constant("t0", 0);
            emit_store_to_frame("t0", offset + static_cast<std::int64_t>(i) * storage.size, storage.size);
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
                    const StorageInfo storage = storage_for_lvalue(index);
                    emit_load_from_address("t0", "t0", 0, storage);
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
            emit_load_constant("t0", static_cast<std::uint64_t>(string_literal_symbol(literal.text).address));
            return;
        }

        throw std::runtime_error("unsupported literal");
    }

    void compile_identifier(const IdentifierExpression& identifier) {
        if (const Symbol* symbol = find_symbol(identifier.name)) {
            if (symbol->is_array) {
                emit_frame_address(symbol->offset);
            } else {
                emit_load_from_frame("t0", symbol->offset, StorageInfo{symbol->element_size, symbol->is_unsigned});
            }
            return;
        }

        if (const GlobalSymbol* symbol = find_global(identifier.name)) {
            if (symbol->is_array) {
                emit_global_address(*symbol);
            } else {
                emit_load_from_global("t0", *symbol);
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
            emit_load_from_address("t0", "t0", 0, storage_for_dereference(*unary.operand));
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
        const StorageInfo storage = storage_for_lvalue(target);
        emit_load_from_address("t1", "t0", 0, storage);
        if (increment) {
            emitter->emit("addi t2, t1, 1");
        } else {
            emit_load_constant("t2", 1);
            emitter->emit("sub t2, t1, t2");
        }
        emit_store_to_address("t2", "t0", 0, storage.size);
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
            const StorageInfo storage = storage_for_lvalue(*assignment.target);
            emit_load_from_address("t2", "t1", 0, storage);
            emit_assignment_operation(assignment.op, "t2", "t0", "t0");
        }

        emit_store_to_address("t0", "t1", 0, storage_for_lvalue(*assignment.target).size);
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
                    if (const Symbol* symbol = find_symbol(identifier.name)) {
                        emit_frame_address(symbol->offset);
                        return;
                    }
                    if (const GlobalSymbol* symbol = find_global(identifier.name)) {
                        emit_global_address(*symbol);
                        return;
                    }
                    fail_at(expression.location, "unknown assignable identifier '" + identifier.name + "'");
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
        std::int64_t element_size = kWordSize;
        if (const auto* identifier = std::get_if<IdentifierExpression>(&index.object->node)) {
            if (const Symbol* symbol = find_symbol(identifier->name)) {
                if (symbol->is_array) {
                    emit_frame_address(symbol->offset);
                    element_size = symbol->element_size;
                } else {
                    compile_expression(*index.object);
                    element_size = symbol->indexed_element_size;
                }
            } else if (const GlobalSymbol* global = find_global(identifier->name)) {
                if (global->is_array) {
                    emit_global_address(*global);
                    element_size = global->element_size;
                } else {
                    compile_expression(*index.object);
                    element_size = global->indexed_element_size;
                }
            } else {
                compile_expression(*index.object);
            }
        } else {
            compile_expression(*index.object);
        }

        push_register("t0");
        compile_expression(*index.index);
        emit_load_constant("t6", static_cast<std::uint64_t>(element_size));
        emitter->emit("mul t0, t0, t6");
        pop_register("t1");
        emitter->emit("add t0, t1, t0");
    }

    [[nodiscard]] StorageInfo storage_for_lvalue(const Expression& expression) const {
        return std::visit(
            Overloaded{
                [this](const IdentifierExpression& identifier) -> StorageInfo {
                    if (const Symbol* symbol = find_symbol(identifier.name)) {
                        return StorageInfo{symbol->element_size, symbol->is_unsigned};
                    }
                    if (const GlobalSymbol* symbol = find_global(identifier.name)) {
                        return StorageInfo{symbol->element_size, symbol->is_unsigned};
                    }
                    return StorageInfo{};
                },
                [](const UnaryExpression&) -> StorageInfo {
                    return StorageInfo{};
                },
                [this](const IndexExpression& index) -> StorageInfo {
                    if (const auto* identifier = std::get_if<IdentifierExpression>(&index.object->node)) {
                        if (const Symbol* symbol = find_symbol(identifier->name); symbol != nullptr && symbol->is_array) {
                            return StorageInfo{symbol->element_size, symbol->is_unsigned};
                        }
                        if (const GlobalSymbol* global = find_global(identifier->name); global != nullptr && global->is_array) {
                            return StorageInfo{global->element_size, global->is_unsigned};
                        }
                        if (const Symbol* symbol = find_symbol(identifier->name)) {
                            return StorageInfo{symbol->indexed_element_size, symbol->indexed_is_unsigned};
                        }
                        if (const GlobalSymbol* global = find_global(identifier->name)) {
                            return StorageInfo{global->indexed_element_size, global->indexed_is_unsigned};
                        }
                    }
                    return StorageInfo{};
                },
                [](const auto&) -> StorageInfo {
                    return StorageInfo{};
                },
            },
            expression.node
        );
    }

    [[nodiscard]] StorageInfo storage_for_lvalue(const IndexExpression& index) const {
        if (const auto* identifier = std::get_if<IdentifierExpression>(&index.object->node)) {
            if (const Symbol* symbol = find_symbol(identifier->name); symbol != nullptr && symbol->is_array) {
                return StorageInfo{symbol->element_size, symbol->is_unsigned};
            }
            if (const GlobalSymbol* global = find_global(identifier->name); global != nullptr && global->is_array) {
                return StorageInfo{global->element_size, global->is_unsigned};
            }
            if (const Symbol* symbol = find_symbol(identifier->name)) {
                return StorageInfo{symbol->indexed_element_size, symbol->indexed_is_unsigned};
            }
            if (const GlobalSymbol* global = find_global(identifier->name)) {
                return StorageInfo{global->indexed_element_size, global->indexed_is_unsigned};
            }
        }
        return StorageInfo{};
    }

    [[nodiscard]] StorageInfo storage_for_dereference(const Expression& expression) const {
        if (const auto* identifier = std::get_if<IdentifierExpression>(&expression.node)) {
            if (const Symbol* symbol = find_symbol(identifier->name)) {
                return StorageInfo{symbol->indexed_element_size, symbol->indexed_is_unsigned};
            }
            if (const GlobalSymbol* global = find_global(identifier->name)) {
                return StorageInfo{global->indexed_element_size, global->indexed_is_unsigned};
            }
        }
        return StorageInfo{};
    }

    [[nodiscard]] bool is_array_identifier(const Expression& expression) const {
        const auto* identifier = std::get_if<IdentifierExpression>(&expression.node);
        if (identifier == nullptr) {
            return false;
        }

        const Symbol* symbol = find_symbol(identifier->name);
        if (symbol != nullptr) {
            return symbol->is_array;
        }

        const GlobalSymbol* global = find_global(identifier->name);
        return global != nullptr && global->is_array;
    }

    [[nodiscard]] const GlobalSymbol* find_global(const std::string& name) const {
        const auto global = globals.find(name);
        if (global == globals.end()) {
            return nullptr;
        }
        return &global->second;
    }

    void emit_frame_address(std::int64_t offset) {
        emit_add_immediate("t0", "s0", offset);
    }

    void emit_load_from_frame(std::string_view reg, std::int64_t offset, StorageInfo storage) {
        ensure_frame_offset(offset);
        emit_load_from_address(reg, "s0", offset, storage);
    }

    void emit_store_to_frame(std::string_view reg, std::int64_t offset, std::int64_t size) {
        ensure_frame_offset(offset);
        emit_store_to_address(reg, "s0", offset, size);
    }

    void emit_global_address(const GlobalSymbol& symbol) {
        emit_load_constant("t0", static_cast<std::uint64_t>(symbol.address));
    }

    void emit_load_from_global(std::string_view reg, const GlobalSymbol& symbol) {
        emit_global_address(symbol);
        emit_load_from_address(reg, "t0", 0, StorageInfo{symbol.element_size, symbol.is_unsigned});
    }

    void emit_load_from_address(std::string_view reg, std::string_view base, std::int64_t offset, StorageInfo storage) {
        const std::string reg_name(reg);
        const std::string base_name(base);
        switch (storage.size) {
        case 1:
            emitter->emit("l8 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            break;
        case 2:
            emitter->emit("l16 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            break;
        case 4:
            emitter->emit("l32 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            break;
        case 8:
            emitter->emit("l64 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            break;
        default:
            throw std::runtime_error("unsupported load size");
        }

        if (storage.is_unsigned && storage.size < kWordSize) {
            emit_load_constant("t6", mask_for_size(storage.size));
            emitter->emit("and " + reg_name + ", " + reg_name + ", t6");
        }
    }

    void emit_store_to_address(std::string_view reg, std::string_view base, std::int64_t offset, std::int64_t size) {
        const std::string reg_name(reg);
        const std::string base_name(base);
        switch (size) {
        case 1:
            emitter->emit("s8 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            return;
        case 2:
            emitter->emit("s16 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            return;
        case 4:
            emitter->emit("s32 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            return;
        case 8:
            emitter->emit("s64 " + reg_name + ", " + base_name + ", " + std::to_string(offset));
            return;
        default:
            throw std::runtime_error("unsupported store size");
        }
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
