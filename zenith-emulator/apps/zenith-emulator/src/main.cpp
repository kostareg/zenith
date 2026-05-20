#include <iostream>

#include "zenith/emulator.hpp"

int main() {
    zenith::emulator::Emulator emulator;
    emulator.reset();

    std::cout << "Zenith emulator native build\n"
              << "Version: " << zenith::emulator::Emulator::version() << '\n';

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
