add_executable(zenith-assembler-app
  src/main.cpp
)

target_link_libraries(zenith-assembler-app
  PRIVATE
    zenith-assembler::zenith-assembler
)
