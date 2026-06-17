#include "zenith/lexer.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <stdexcept>

namespace zenith::assembler {

std::vector<Token> Lexer::lex() const {
    std::vector<Token> tts;

    for (size_t i = 0; i < bytes.size(); ++i) {
        switch (bytes[i]) {
        case ' ':
            continue;
        case '#': {
            ++i;
            while (i < bytes.size() && bytes[i] != '\n')
                ++i;
            tts.push_back(make_single_char_token(TokenType::NewLine, i));
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
            if (isalpha(bytes[i]) || bytes[i] == '_') {
                std::string name;
                name.push_back(bytes[i]);
                size_t start = i;
                ++i;
                while (i < bytes.size() && (isalpha(bytes[i]) || isdigit(bytes[i]) || bytes[i] == '_')) {
                    name.push_back(bytes[i]);
                    ++i;
                }
                size_t end = --i;

                tts.emplace_back(TokenType::Ident, &bytes[start], end - start, name, 0);
                continue;
            } else if (isdigit(bytes[i]) || bytes[i] == '-') {
                std::string val;
                const size_t start = i;
                val.push_back(bytes[i]);
                ++i;
                if (val == "-" && (i >= bytes.size() || !isdigit(bytes[i]))) {
                    throw std::runtime_error("expected digit after '-'");
                }
                while (i < bytes.size() && (isxdigit(bytes[i]) || bytes[i] == 'x' || bytes[i] == 'X')) {
                    val.push_back(bytes[i]);
                    ++i;
                }
                const size_t end = --i;

                errno = 0;
                char* parse_end = nullptr;
                const long long number = std::strtoll(val.c_str(), &parse_end, 0);
                if (errno == ERANGE || parse_end == nullptr || *parse_end != '\0') {
                    throw std::runtime_error("invalid numeric literal");
                }
                tts.emplace_back(TokenType::Number, &bytes[start], end - start, "", static_cast<std::int64_t>(number));
                continue;
            }
            std::cerr << bytes[i];
            throw std::runtime_error("unknown character");
        }
        }
    }

    return tts;
}

} // namespace zenith::assembler
