#include "zenith/assembler.hpp"

#include "zenith/compiler.hpp"
#include "zenith/lexer.hpp"
#include "zenith/parser.hpp"

#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace zenith::assembler {

std::string_view Assembler::version() noexcept {
    return "0.1.0";
}

std::string format_machine_code(const std::vector<std::uint32_t>& machine_code) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < machine_code.size(); ++i) {
        if (i > 0) output << '\n';
        output << "0x" << std::setw(8) << machine_code[i];
    }

    return output.str();
}

ProgramImage compile_source(std::string source) {
    std::vector<unsigned char> bytes(source.begin(), source.end());
    Lexer lexer(std::move(bytes));
    Parser parser(lexer.lex());
    Compiler compiler(parser.parse());
    return compiler.compile_program();
}

std::string Assembler::assemble(std::string source) const {
    return format_machine_code(compile_source(std::move(source)).code);
}

AssembledProgram Assembler::assemble_program(std::string source) const {
    ProgramImage image = compile_source(std::move(source));
    return AssembledProgram{format_machine_code(image.code), std::move(image.data)};
}

} // namespace zenith::assembler

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

namespace {

emscripten::val data_to_js_array(const std::vector<std::uint8_t>& data) {
    auto result = emscripten::val::array();
    for (std::size_t i = 0; i < data.size(); ++i) {
        result.set(i, data[i]);
    }
    return result;
}

} // namespace

emscripten::val assemble_program(zenith::assembler::Assembler& assembler, std::string source) {
    const zenith::assembler::AssembledProgram program = assembler.assemble_program(std::move(source));
    emscripten::val result = emscripten::val::object();
    result.set("machineCode", program.machine_code);
    result.set("data", data_to_js_array(program.data));
    return result;
}

std::string version() {
    return std::string(zenith::assembler::Assembler::version());
}

EMSCRIPTEN_BINDINGS(zenith_assembler) {
    emscripten::class_<zenith::assembler::Assembler>("Assembler")
        .constructor<>()
        .function("assemble", &zenith::assembler::Assembler::assemble)
        .function("assembleProgram", &assemble_program)
        .class_function("version", &version);
}

#endif // __EMSCRIPTEN__
