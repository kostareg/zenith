#include <iostream>
#include <vector>

#include "zenith/assembler.hpp"
#include "zenith/compiler.hpp"
#include "zenith/lexer.hpp"
#include "zenith/parser.hpp"

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

# define exact data in data
.data
  label: .byte 100)";
    std::vector<unsigned char> test_program_v(test_program.begin(), test_program.end());

    Lexer lexer(test_program_v);

    std::cout << "------ lexer ------" << std::endl;

    auto lexed = lexer.lex();
    for (Token& t : lexed) {
        t.display();
    }

    std::cout << "------ parser ------" << std::endl;

    Parser parser(lexed);
    auto parsed = parser.parse();

    std::cout << parsed << std::endl;
  
    std::cout << "------ compiler ------" << std::endl;

    Compiler compiler(parsed);
    auto compiled = compiler.compile();

    std::cout << std::hex;
    for (auto& x : compiled) {
      std::cout << "0x" << x << std::endl;
    }
    std::cout << std::dec;

    return 0;
}
