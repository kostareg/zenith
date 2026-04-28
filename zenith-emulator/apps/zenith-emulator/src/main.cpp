#include <iostream>

#include "zenith_emulator/zenith_emulator.h"

int main() {
  zenith_emulator::Emulator emulator;
  emulator.Reset();
  emulator.Step();

  std::cout
      << "Zenith emulator bootstrap\n"
      << "version: " << zenith_emulator::Emulator::Version() << '\n'
      << "accumulator: " << emulator.accumulator() << '\n'
      << "2 + 3 = " << zenith_emulator::Add(2, 3) << '\n';

  return 0;
}
