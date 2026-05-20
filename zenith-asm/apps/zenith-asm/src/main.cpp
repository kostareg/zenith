#include <iostream>
#include <vector>

#include "zenith/assembler.hpp"
#include "zenith/lexer.hpp"

using namespace zenith::assembler;

int main() {
    std::cout << "Zenith assembler" << std::endl;

    std::string test_program = R"(# lines that start with # are comments
# start with .main
.main
  add r0, r0, r1
  # define a label before a section to reference it later
  mylabel: add r1, r1, r1
  j mylabel # desugars to jal ...

# define exact data in bss
.bss
  label: .byte 100)";
    std::vector<unsigned char> test_program_v(test_program.begin(), test_program.end());

    Lexer lexer(test_program_v);

    for (Token t : lexer.lex()) {
        t.display();
    }

    return 0;
}
