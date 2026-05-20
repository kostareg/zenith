add_executable(zenith-asm-app
  src/main.cpp
)

target_link_libraries(zenith-asm-app
  PRIVATE
    zenith-asm::zenith-asm
)
