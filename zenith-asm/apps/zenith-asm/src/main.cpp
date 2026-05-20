#include <iostream>
#include <vector>
#include <fstream>

#include "zenith/assembler.hpp"
#include "zenith/compiler.hpp"
#include "zenith/lexer.hpp"
#include "zenith/parser.hpp"

using namespace zenith::assembler;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "file not specified or too many arguments" << std::endl;
        return 1;
    }

    std::cout << "Zenith assembler" << std::endl
              << "assembling " << argv[1] << std::endl;

    std::ifstream file(argv[1]);
    
    auto program = std::vector<unsigned char>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    Lexer lexer(program);

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

    // todo: .data section

    return 0;
}
