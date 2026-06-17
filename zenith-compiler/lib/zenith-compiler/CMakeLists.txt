add_library(zenith-compiler-compile-options INTERFACE)
add_library(zenith-compiler::compile-options ALIAS zenith-compiler-compile-options)

target_compile_features(zenith-compiler-compile-options INTERFACE cxx_std_20)

add_library(zenith-compiler
  src/zenith/compiler.cpp
  src/zenith/lexer.cpp
  src/zenith/parser.cpp
  src/zenith/preprocessor.cpp
)

add_library(zenith-compiler::zenith-compiler ALIAS zenith-compiler)

target_link_libraries(zenith-compiler
  PUBLIC
    zenith-compiler::compile-options
)

target_include_directories(zenith-compiler
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
