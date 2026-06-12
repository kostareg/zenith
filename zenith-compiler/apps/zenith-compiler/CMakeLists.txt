add_executable(zenith-compiler-app
  src/main.cpp
)

target_link_libraries(zenith-compiler-app
  PRIVATE
    zenith-compiler::zenith-compiler
)
