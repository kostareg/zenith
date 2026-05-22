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

std::string Assembler::assemble(std::string source) const {
    std::vector<unsigned char> bytes(source.begin(), source.end());
    Lexer lexer(std::move(bytes));
    Parser parser(lexer.lex());
    Compiler compiler(parser.parse());
    const std::vector<std::uint32_t> machine_code = compiler.compile();

    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < machine_code.size(); ++i) {
        if (i > 0) output << '\n';
        output << "0x" << std::setw(8) << machine_code[i];
    }

    return output.str();
}

} // namespace zenith::assembler

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

std::string version() {
    return std::string(zenith::assembler::Assembler::version());
}

EMSCRIPTEN_BINDINGS(zenith_assembler) {
    emscripten::class_<zenith::assembler::Assembler>("Assembler")
        .constructor<>()
        .function("assemble", &zenith::assembler::Assembler::assemble)
        .class_function("version", &version);
}

#endif // __EMSCRIPTEN__
