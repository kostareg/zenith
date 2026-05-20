include(FetchContent)

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(zenith-asm-tests
  assembler_tests.cpp
  compiler_tests.cpp
  format_tests.cpp
  lexer_tests.cpp
  parser_tests.cpp
)

target_link_libraries(zenith-asm-tests
  PRIVATE
    zenith-asm::zenith-asm
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zenith-asm-tests)
