#include <iostream>

#include "zenith_emulator/zenith_emulator.h"

int main() {
  zenith_emulator::Emulator emulator;
  emulator.Reset();

  std::cout
      << "Zenith emulator bootstrap\n"
      << "version: " << zenith_emulator::Emulator::Version() << '\n'
      << "accumulator: " << emulator.accumulator() << '\n'
      << "2 + 3 = " << zenith_emulator::Add(2, 3) << '\n';

  uint32_t i;
  while (std::cout << "> 0x", std::cin >> std::hex >> i) {
    emulator.Step(i);
    std::cout << std::dec;
  }

  auto registers = emulator.GetRegisters();
  for (int i = 0; i < 32; ++i) {
    std::cout << "register " << i << ": " << registers[i] << std::endl;
  }

  return 0;
}
