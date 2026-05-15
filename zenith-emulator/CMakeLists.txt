cmake_minimum_required(VERSION 3.21)

project(zenith-emulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZENITH_EMULATOR_BUILD_APPS "Build native Zenith emulator applications" ON)
option(ZENITH_EMULATOR_BUILD_WASM "Build the Emscripten JS/WASM module" OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(lib/zenith-emulator)

if(ZENITH_EMULATOR_BUILD_APPS)
  add_subdirectory(apps/zenith-emulator)
endif()

if(ZENITH_EMULATOR_BUILD_WASM)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "ZENITH_EMULATOR_BUILD_WASM=ON requires the Emscripten toolchain.")
  endif()

  add_executable(zenith-emulator-wasm
    lib/zenith-emulator/src/zenith_emulator.cpp
  )

  target_link_libraries(zenith-emulator-wasm PRIVATE zenith-emulator::compile-options)
  target_include_directories(zenith-emulator-wasm
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/zenith-emulator/include
  )

  set_target_properties(zenith-emulator-wasm PROPERTIES
    OUTPUT_NAME zenith-emulator
    SUFFIX ".js"
  )

  target_link_options(zenith-emulator-wasm PRIVATE
    "SHELL:-s WASM=1"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s ENVIRONMENT=web,worker,node"
    "SHELL:-lembind"
  )
endif()
