#include "zenith/assembler.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace zenith::assembler {

std::string_view Assembler::version() noexcept {
    return "0.1.0";
}

} // namespace zenith::assembler

#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>

std::string version() {
    return std::string(zenith::assembler::Assembler::version());
}

EMSCRIPTEN_BINDINGS(zenith_asm) {
    emscripten::class_<zenith::assembler::Assembler>("Assembler")
        .constructor<>()
        .class_function("version", &version);
}

#endif // __EMSCRIPTEN__
