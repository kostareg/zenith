#pragma once

#include <stdint.h>

#if defined(_WIN32)
  #if defined(ZENITH_EMULATOR_BUILD_DLL)
    #define ZENITH_EMULATOR_EXPORT __declspec(dllexport)
  #else
    #define ZENITH_EMULATOR_EXPORT
  #endif
#elif defined(__EMSCRIPTEN__)
  #include <emscripten/emscripten.h>
  #define ZENITH_EMULATOR_EXPORT EMSCRIPTEN_KEEPALIVE
#else
  #define ZENITH_EMULATOR_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

ZENITH_EMULATOR_EXPORT uint32_t zenith_emulator_add(uint32_t lhs, uint32_t rhs);
ZENITH_EMULATOR_EXPORT void zenith_emulator_reset(void);
ZENITH_EMULATOR_EXPORT void zenith_emulator_step(uint32_t);
ZENITH_EMULATOR_EXPORT uint64_t zenith_emulator_version(void);

#ifdef __cplusplus
}
#endif
