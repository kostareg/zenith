#include "zenith/lexer.hpp"

namespace zenith::assembler {

std::vector<Token> Lexer::lex() {
    std::vector<Token> tts;

    for (size_t i = 0; i < bytes.size(); ++i) {
        switch (bytes[i]) {
            case ' ': continue;
            case '#': {
                ++i;
                while (i < bytes.size() && bytes[i] != '\n') ++i;
                continue;
            }
            case '\n': {
                tts.push_back(make_single_char_token(TokenType::NewLine, i));
                continue;
            }
            case ':': {
                tts.push_back(make_single_char_token(TokenType::Colon, i));
                continue;
            }
            case '.': {
                tts.push_back(make_single_char_token(TokenType::Dot, i));
                continue;
            }
            case ',': {
                tts.push_back(make_single_char_token(TokenType::Comma, i));
                continue;
            }
            default: {
                if (isalpha(bytes[i])) {
                    std::string name;
                    name.push_back(bytes[i]);
                    size_t start = i;
                    ++i;
                    while (i < bytes.size() && (isalpha(bytes[i]) || isdigit(bytes[i]))) {
                        name.push_back(bytes[i]);
                        ++i;
                    }
                    size_t end = --i;

                    tts.emplace_back(TokenType::Ident, &bytes[start], end - start, name, 0);
                    continue;
                } else if (isdigit(bytes[i])) {
                    // todo: hex, floats...
                    std::string val;
                    val.push_back(bytes[i]);
                    size_t start = i;
                    ++i;
                    while (i < bytes.size() && isdigit(bytes[i])) {
                        val.push_back(bytes[i]);
                        ++i;
                    }
                    size_t end = --i;
                    tts.emplace_back(TokenType::Number, &bytes[start], end - start, "", std::stod(val));
                    continue;
                }
                std::cerr << bytes[i];
                throw std::runtime_error("unknown character");
            }
        }
    }

    return tts;
}

}
