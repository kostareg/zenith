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

add_executable(zenith-assembler-tests
  assembler_tests.cpp
  compiler_tests.cpp
  format_tests.cpp
  lexer_tests.cpp
  parser_tests.cpp
)

target_link_libraries(zenith-assembler-tests
  PRIVATE
    zenith-assembler::zenith-assembler
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zenith-assembler-tests)
