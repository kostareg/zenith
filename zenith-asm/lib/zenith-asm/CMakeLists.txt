add_library(zenith-asm-compile-options INTERFACE)
add_library(zenith-asm::compile-options ALIAS zenith-asm-compile-options)

target_compile_features(zenith-asm-compile-options INTERFACE cxx_std_20)

add_library(zenith-asm
  src/zenith/assembler.cpp
)

add_library(zenith-asm::zenith-asm ALIAS zenith-asm)

target_link_libraries(zenith-asm
  PUBLIC
    zenith-asm::compile-options
)

target_include_directories(zenith-asm
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
