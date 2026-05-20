cmake_minimum_required(VERSION 3.21)

project(zenith-asm VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZENITH_ASM_BUILD_APPS "Build native Zenith asm applications" ON)
option(ZENITH_ASM_BUILD_WASM "Build the Emscripten JS/WASM module" OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(lib/zenith-asm)

if(ZENITH_ASM_BUILD_APPS)
  add_subdirectory(apps/zenith-asm)
endif()

if(ZENITH_ASM_BUILD_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "ZENITH_ASM_BUILD_WASM=ON requires the Emscripten toolchain.")
  endif()

  add_executable(zenith-asm-wasm
    lib/zenith-asm/src/zenith/assembler.cpp
  )

  target_link_libraries(zenith-asm-wasm PRIVATE zenith-asm::compile-options)
  target_include_directories(zenith-asm-wasm
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/zenith-asm/include
  )

  set_target_properties(zenith-asm-wasm PROPERTIES
    OUTPUT_NAME zenith-asm
    SUFFIX ".js"
  )

  target_link_options(zenith-asm-wasm PRIVATE
    "SHELL:-s WASM=1"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s ENVIRONMENT=web,worker,node"
    "SHELL:-lembind"
    "SHELL:--emit-tsd zenith-asm.d.ts"
  )
endif()
