#include <iostream>

#include "zenith_emulator/zenith_emulator.hpp"

int main() {
    zenith_emulator::Emulator emulator;
    emulator.reset();

    std::cout << "Zenith emulator bootstrap\n"
              << "version: " << zenith_emulator::Emulator::version() << '\n'
              << "2 + 3 = " << zenith_emulator::Emulator::add(2, 3) << '\n';

    uint32_t i;
    while (std::cout << "> 0x", std::cin >> std::hex >> i) {
        emulator.step(i);
        std::cout << std::dec;
    }

    auto registers = emulator.get_registers();
    for (int i = 0; i < 32; ++i) {
        std::cout << "register " << i << ": " << registers[i] << std::endl;
    }

    return 0;
}
