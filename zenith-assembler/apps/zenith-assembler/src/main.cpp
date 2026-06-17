#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "zenith/compiler.hpp"
#include "zenith/format.hpp"
#include "zenith/lexer.hpp"
#include "zenith/parser.hpp"

using namespace zenith::assembler;

int main(int argc, char* argv[]) {
    constexpr const char* kOutputPath = "out.zelf";
    constexpr const char* kDefaultProgramName = "example";

    if (argc != 2) {
        std::cerr << "usage: zenith-assembler-app <input.zasm>" << std::endl;
        return 1;
    }

    try {
        std::cout << "Zenith assembler" << std::endl << "assembling " << argv[1] << std::endl;

        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            std::cerr << "failed to open input file: " << argv[1] << std::endl;
            return 1;
        }

        auto program =
            std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

        Lexer lexer(program);
        Parser parser(lexer.lex());
        Compiler compiler(parser.parse());
        const ProgramImage compiled = compiler.compile_program();

        const std::vector<std::uint8_t> zelf = Format(kDefaultProgramName, compiled).format();

        std::ofstream output(kOutputPath, std::ios::binary);
        if (!output) {
            std::cerr << "failed to open output file: " << kOutputPath << std::endl;
            return 1;
        }

        output.write(reinterpret_cast<const char*>(zelf.data()), static_cast<std::streamsize>(zelf.size()));
        if (!output) {
            std::cerr << "failed to write output file: " << kOutputPath << std::endl;
            return 1;
        }

        std::cout << "wrote " << kOutputPath << " (" << zelf.size() << " bytes)" << std::endl;
    } catch (const std::exception& error) {
        std::cerr << "assembly failed: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}
