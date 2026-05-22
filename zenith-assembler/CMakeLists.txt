cmake_minimum_required(VERSION 3.21)

project(zenith-assembler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZENITH_ASSEMBLER_BUILD_APPS "Build native Zenith assembler applications" ON)
option(ZENITH_ASSEMBLER_BUILD_WASM "Build the Emscripten JS/WASM module" OFF)
option(ZENITH_ASSEMBLER_BUILD_TESTS "Build Zenith assembler unit tests" ON)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(lib/zenith-assembler)

if(ZENITH_ASSEMBLER_BUILD_APPS)
  add_subdirectory(apps/zenith-assembler)
endif()

if(ZENITH_ASSEMBLER_BUILD_TESTS AND NOT EMSCRIPTEN)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()

if(ZENITH_ASSEMBLER_BUILD_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "ZENITH_ASSEMBLER_BUILD_WASM=ON requires the Emscripten toolchain.")
  endif()

  add_executable(zenith-assembler-wasm
    lib/zenith-assembler/src/zenith/assembler.cpp
    lib/zenith-assembler/src/zenith/compiler.cpp
    lib/zenith-assembler/src/zenith/lexer.cpp
    lib/zenith-assembler/src/zenith/parser.cpp
  )

  target_link_libraries(zenith-assembler-wasm PRIVATE zenith-assembler::compile-options)
  target_include_directories(zenith-assembler-wasm
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/zenith-assembler/include
  )

  set_target_properties(zenith-assembler-wasm PROPERTIES
    OUTPUT_NAME zenith-assembler
    SUFFIX ".js"
  )

  target_link_options(zenith-assembler-wasm PRIVATE
    "SHELL:-s WASM=1"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s ENVIRONMENT=web,worker,node"
    "SHELL:-s NO_DISABLE_EXCEPTION_CATCHING=1"
    "SHELL:-lembind"
    "SHELL:--emit-tsd zenith-assembler.d.ts"
  )
endif()
