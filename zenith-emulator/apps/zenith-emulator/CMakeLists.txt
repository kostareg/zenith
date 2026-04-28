add_executable(zenith-emulator-app
  src/main.cpp
)

target_link_libraries(zenith-emulator-app
  PRIVATE
    zenith-emulator::zenith-emulator
)
