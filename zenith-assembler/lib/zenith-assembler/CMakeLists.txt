add_library(zenith-assembler-compile-options INTERFACE)
add_library(zenith-assembler::compile-options ALIAS zenith-assembler-compile-options)

target_compile_features(zenith-assembler-compile-options INTERFACE cxx_std_20)

add_library(zenith-assembler
  src/zenith/assembler.cpp
  src/zenith/compiler.cpp
  src/zenith/format.cpp
  src/zenith/lexer.cpp
  src/zenith/parser.cpp
)

add_library(zenith-assembler::zenith-assembler ALIAS zenith-assembler)

target_link_libraries(zenith-assembler
  PUBLIC
    zenith-assembler::compile-options
)

target_include_directories(zenith-assembler
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
