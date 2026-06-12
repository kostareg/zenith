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

add_executable(zenith-compiler-tests
  compiler_tests.cpp
  lexer_tests.cpp
)

target_link_libraries(zenith-compiler-tests
  PRIVATE
    zenith-compiler::zenith-compiler
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zenith-compiler-tests)
