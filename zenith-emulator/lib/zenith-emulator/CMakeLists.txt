add_library(zenith-emulator-compile-options INTERFACE)
add_library(zenith-emulator::compile-options ALIAS zenith-emulator-compile-options)

target_compile_features(zenith-emulator-compile-options INTERFACE cxx_std_20)

add_library(zenith-emulator
  src/zenith/emulator.cpp
)

add_library(zenith-emulator::zenith-emulator ALIAS zenith-emulator)

target_link_libraries(zenith-emulator
  PUBLIC
    zenith-emulator::compile-options
)

target_include_directories(zenith-emulator
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
