#include "zenith/compiler.hpp"

namespace zenith::compiler {

std::string_view Compiler::version() noexcept {
    return "0.1.0";
}

} // namespace zenith::compiler

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

std::string version() {
    return std::string(zenith::compiler::Compiler::version());
}

EMSCRIPTEN_BINDINGS(zenith_compiler) {
    emscripten::class_<zenith::compiler::Compiler>("Compiler").constructor<>().class_function("version", &version);
}

#endif // __EMSCRIPTEN__
