#include "zenith/lexer.hpp"

#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace zenith::compiler {
namespace {

[[nodiscard]] bool is_alpha(unsigned char ch) noexcept {
    return std::isalpha(ch) != 0;
}

[[nodiscard]] bool is_digit(unsigned char ch) noexcept {
    return std::isdigit(ch) != 0;
}

[[nodiscard]] bool is_hex_digit(unsigned char ch) noexcept {
    return std::isxdigit(ch) != 0;
}

[[nodiscard]] bool is_oct_digit(unsigned char ch) noexcept {
    return ch >= '0' && ch <= '7';
}

[[nodiscard]] bool is_identifier_start(unsigned char ch) noexcept {
    return is_alpha(ch) || ch == '_';
}

[[nodiscard]] bool is_identifier_body(unsigned char ch) noexcept {
    return is_identifier_start(ch) || is_digit(ch);
}

[[nodiscard]] int hex_value(unsigned char ch) noexcept {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return ch - 'A' + 10;
}

[[nodiscard]] TokenType keyword_or_identifier(std::string_view lexeme) noexcept {
    if (lexeme == "if") {
        return TokenType::KeywordIf;
    }
    if (lexeme == "else") {
        return TokenType::KeywordElse;
    }
    if (lexeme == "while") {
        return TokenType::KeywordWhile;
    }
    if (lexeme == "for") {
        return TokenType::KeywordFor;
    }
    if (lexeme == "break") {
        return TokenType::KeywordBreak;
    }
    if (lexeme == "continue") {
        return TokenType::KeywordContinue;
    }
    if (lexeme == "return") {
        return TokenType::KeywordReturn;
    }
    if (lexeme == "switch") {
        return TokenType::KeywordSwitch;
    }
    if (lexeme == "case") {
        return TokenType::KeywordCase;
    }
    if (lexeme == "default") {
        return TokenType::KeywordDefault;
    }
    if (lexeme == "struct") {
        return TokenType::KeywordStruct;
    }
    if (lexeme == "enum") {
        return TokenType::KeywordEnum;
    }
    if (lexeme == "static") {
        return TokenType::KeywordStatic;
    }
    if (lexeme == "true") {
        return TokenType::KeywordTrue;
    }
    if (lexeme == "false") {
        return TokenType::KeywordFalse;
    }
    if (lexeme == "bool") {
        return TokenType::KeywordBool;
    }
    if (lexeme == "void") {
        return TokenType::KeywordVoid;
    }
    if (lexeme == "char") {
        return TokenType::KeywordChar;
    }
    if (lexeme == "short") {
        return TokenType::KeywordShort;
    }
    if (lexeme == "int") {
        return TokenType::KeywordInt;
    }
    if (lexeme == "long") {
        return TokenType::KeywordLong;
    }
    if (lexeme == "signed") {
        return TokenType::KeywordSigned;
    }
    if (lexeme == "unsigned") {
        return TokenType::KeywordUnsigned;
    }
    if (lexeme == "int8_t") {
        return TokenType::KeywordInt8;
    }
    if (lexeme == "int16_t") {
        return TokenType::KeywordInt16;
    }
    if (lexeme == "int32_t") {
        return TokenType::KeywordInt32;
    }
    if (lexeme == "int64_t") {
        return TokenType::KeywordInt64;
    }
    if (lexeme == "uint8_t") {
        return TokenType::KeywordUInt8;
    }
    if (lexeme == "uint16_t") {
        return TokenType::KeywordUInt16;
    }
    if (lexeme == "uint32_t") {
        return TokenType::KeywordUInt32;
    }
    if (lexeme == "uint64_t") {
        return TokenType::KeywordUInt64;
    }

    return TokenType::Identifier;
}

class Scanner {
  public:
    explicit Scanner(std::string_view source)
        : source(source) {}

    [[nodiscard]] std::vector<Token> lex() {
        while (!is_at_end()) {
            skip_whitespace_and_comments();
            if (!is_at_end()) {
                scan_token();
            }
        }

        tokens.push_back(Token{TokenType::EndOfFile, "", line, column, 0, ""});
        return tokens;
    }

  private:
    std::string_view source;
    std::vector<Token> tokens;
    std::size_t current = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    [[nodiscard]] bool is_at_end() const noexcept {
        return current >= source.size();
    }

    [[nodiscard]] unsigned char peek() const noexcept {
        if (is_at_end()) {
            return '\0';
        }
        return static_cast<unsigned char>(source[current]);
    }

    [[nodiscard]] unsigned char peek_next() const noexcept {
        if (current + 1 >= source.size()) {
            return '\0';
        }
        return static_cast<unsigned char>(source[current + 1]);
    }

    unsigned char advance() noexcept {
        const unsigned char ch = peek();
        ++current;

        if (ch == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }

        return ch;
    }

    bool match(unsigned char expected) noexcept {
        if (peek() != expected) {
            return false;
        }
        advance();
        return true;
    }

    void skip_whitespace_and_comments() {
        for (;;) {
            switch (peek()) {
            case ' ':
            case '\r':
            case '\t':
            case '\f':
            case '\v':
            case '\n':
                advance();
                continue;
            case '/':
                if (peek_next() == '/') {
                    while (!is_at_end() && peek() != '\n') {
                        advance();
                    }
                    continue;
                }
                if (peek_next() == '*') {
                    advance();
                    advance();
                    skip_block_comment();
                    continue;
                }
                return;
            default:
                return;
            }
        }
    }

    void skip_block_comment() {
        while (!is_at_end()) {
            if (peek() == '*' && peek_next() == '/') {
                advance();
                advance();
                return;
            }
            advance();
        }

        fail("unterminated block comment");
    }

    void scan_token() {
        const std::size_t start = current;
        const std::size_t start_line = line;
        const std::size_t start_column = column;
        const unsigned char ch = advance();

        switch (ch) {
        case '(':
            add_token(TokenType::LeftParen, start, start_line, start_column);
            return;
        case ')':
            add_token(TokenType::RightParen, start, start_line, start_column);
            return;
        case '{':
            add_token(TokenType::LeftBrace, start, start_line, start_column);
            return;
        case '}':
            add_token(TokenType::RightBrace, start, start_line, start_column);
            return;
        case '[':
            add_token(TokenType::LeftBracket, start, start_line, start_column);
            return;
        case ']':
            add_token(TokenType::RightBracket, start, start_line, start_column);
            return;
        case ';':
            add_token(TokenType::Semicolon, start, start_line, start_column);
            return;
        case ',':
            add_token(TokenType::Comma, start, start_line, start_column);
            return;
        case '.':
            if (is_digit(peek())) {
                fail_at(start_line, start_column, "floating-point literals are not supported");
            }
            add_token(TokenType::Dot, start, start_line, start_column);
            return;
        case ':':
            add_token(TokenType::Colon, start, start_line, start_column);
            return;
        case '?':
            add_token(TokenType::Question, start, start_line, start_column);
            return;
        case '+':
            if (match('+')) {
                add_token(TokenType::PlusPlus, start, start_line, start_column);
            } else if (match('=')) {
                add_token(TokenType::PlusEqual, start, start_line, start_column);
            } else {
                add_token(TokenType::Plus, start, start_line, start_column);
            }
            return;
        case '-':
            if (match('-')) {
                add_token(TokenType::MinusMinus, start, start_line, start_column);
            } else if (match('=')) {
                add_token(TokenType::MinusEqual, start, start_line, start_column);
            } else if (match('>')) {
                add_token(TokenType::Arrow, start, start_line, start_column);
            } else {
                add_token(TokenType::Minus, start, start_line, start_column);
            }
            return;
        case '*':
            add_token(match('=') ? TokenType::StarEqual : TokenType::Star, start, start_line, start_column);
            return;
        case '/':
            add_token(match('=') ? TokenType::SlashEqual : TokenType::Slash, start, start_line, start_column);
            return;
        case '%':
            add_token(match('=') ? TokenType::PercentEqual : TokenType::Percent, start, start_line, start_column);
            return;
        case '=':
            add_token(match('=') ? TokenType::EqualEqual : TokenType::Equal, start, start_line, start_column);
            return;
        case '!':
            add_token(match('=') ? TokenType::BangEqual : TokenType::Bang, start, start_line, start_column);
            return;
        case '<':
            scan_less(start, start_line, start_column);
            return;
        case '>':
            scan_greater(start, start_line, start_column);
            return;
        case '~':
            add_token(TokenType::Tilde, start, start_line, start_column);
            return;
        case '&':
            if (match('&')) {
                add_token(TokenType::AmpersandAmpersand, start, start_line, start_column);
            } else if (match('=')) {
                add_token(TokenType::AmpersandEqual, start, start_line, start_column);
            } else {
                add_token(TokenType::Ampersand, start, start_line, start_column);
            }
            return;
        case '|':
            if (match('|')) {
                add_token(TokenType::PipePipe, start, start_line, start_column);
            } else if (match('=')) {
                add_token(TokenType::PipeEqual, start, start_line, start_column);
            } else {
                add_token(TokenType::Pipe, start, start_line, start_column);
            }
            return;
        case '^':
            add_token(match('=') ? TokenType::CaretEqual : TokenType::Caret, start, start_line, start_column);
            return;
        case '\'':
            scan_character_literal(start, start_line, start_column);
            return;
        case '"':
            scan_string_literal(start, start_line, start_column);
            return;
        default:
            if (is_identifier_start(ch)) {
                scan_identifier(start, start_line, start_column);
                return;
            }
            if (is_digit(ch)) {
                scan_integer_literal(start, start_line, start_column, ch);
                return;
            }

            fail_at(start_line, start_column, "unknown character");
        }
    }

    void scan_less(std::size_t start, std::size_t start_line, std::size_t start_column) {
        if (match('<')) {
            add_token(match('=') ? TokenType::LessLessEqual : TokenType::LessLess, start, start_line, start_column);
            return;
        }

        add_token(match('=') ? TokenType::LessEqual : TokenType::Less, start, start_line, start_column);
    }

    void scan_greater(std::size_t start, std::size_t start_line, std::size_t start_column) {
        if (match('>')) {
            add_token(
                match('=') ? TokenType::GreaterGreaterEqual : TokenType::GreaterGreater, start, start_line, start_column
            );
            return;
        }

        add_token(match('=') ? TokenType::GreaterEqual : TokenType::Greater, start, start_line, start_column);
    }

    void scan_identifier(std::size_t start, std::size_t start_line, std::size_t start_column) {
        while (is_identifier_body(peek())) {
            advance();
        }

        const std::string_view lexeme = source.substr(start, current - start);
        add_token(keyword_or_identifier(lexeme), start, start_line, start_column);
    }

    void
    scan_integer_literal(std::size_t start, std::size_t start_line, std::size_t start_column, unsigned char first) {
        std::uint64_t value = 0;

        if (first == '0' && (peek() == 'x' || peek() == 'X')) {
            advance();
            value = scan_digits(16, "hexadecimal integer literal requires at least one digit");
        } else if (first == '0' && (peek() == 'b' || peek() == 'B')) {
            fail_at(start_line, start_column, "binary integer literals are not supported");
        } else if (first == '0') {
            value = scan_prefixed_octal_digits();
        } else {
            value = static_cast<std::uint64_t>(first - '0');
            while (is_digit(peek())) {
                value = checked_append_digit(value, 10, advance() - '0');
            }
        }

        if (peek() == '.' || peek() == 'e' || peek() == 'E' || peek() == 'p' || peek() == 'P') {
            fail_at(start_line, start_column, "floating-point literals are not supported");
        }

        scan_integer_suffix(start_line, start_column);

        if (is_identifier_body(peek())) {
            fail_at(start_line, start_column, "invalid integer literal suffix");
        }

        add_token(TokenType::IntegerLiteral, start, start_line, start_column, value);
    }

    std::uint64_t scan_digits(int base, std::string_view missing_digits_message) {
        bool has_digit = false;
        std::uint64_t value = 0;

        while (true) {
            const unsigned char ch = peek();
            int digit = 0;

            if (base == 16 && is_hex_digit(ch)) {
                digit = hex_value(ch);
            } else if (base == 10 && is_digit(ch)) {
                digit = ch - '0';
            } else if (base == 8 && is_oct_digit(ch)) {
                digit = ch - '0';
            } else {
                break;
            }

            advance();
            has_digit = true;
            value = checked_append_digit(value, base, digit);
        }

        if (!has_digit) {
            fail(std::string(missing_digits_message));
        }

        return value;
    }

    std::uint64_t scan_prefixed_octal_digits() {
        std::uint64_t value = 0;

        while (is_digit(peek())) {
            if (!is_oct_digit(peek())) {
                fail("invalid digit in octal integer literal");
            }
            value = checked_append_digit(value, 8, advance() - '0');
        }

        return value;
    }

    void scan_integer_suffix(std::size_t start_line, std::size_t start_column) {
        bool has_unsigned = false;
        bool has_long = false;

        while (is_identifier_start(peek())) {
            const unsigned char ch = peek();
            if ((ch == 'u' || ch == 'U') && !has_unsigned) {
                has_unsigned = true;
                advance();
                continue;
            }

            if ((ch == 'l' || ch == 'L') && !has_long) {
                has_long = true;
                advance();
                if (peek() == 'l' || peek() == 'L') {
                    advance();
                }
                continue;
            }

            fail_at(start_line, start_column, "invalid integer literal suffix");
        }
    }

    void scan_character_literal(std::size_t start, std::size_t start_line, std::size_t start_column) {
        if (peek() == '\'') {
            fail_at(start_line, start_column, "empty character literal");
        }
        if (peek() == '\n' || is_at_end()) {
            fail_at(start_line, start_column, "unterminated character literal");
        }

        const unsigned char value = read_character();

        if (!match('\'')) {
            fail_at(start_line, start_column, "character literal must contain one character");
        }

        add_token(TokenType::CharacterLiteral, start, start_line, start_column, value, std::string(1, value));
    }

    void scan_string_literal(std::size_t start, std::size_t start_line, std::size_t start_column) {
        std::string text;

        while (!is_at_end()) {
            if (peek() == '"') {
                advance();
                add_token(TokenType::StringLiteral, start, start_line, start_column, 0, std::move(text));
                return;
            }

            if (peek() == '\n') {
                fail_at(start_line, start_column, "unterminated string literal");
            }

            text.push_back(static_cast<char>(read_character()));
        }

        fail_at(start_line, start_column, "unterminated string literal");
    }

    unsigned char read_character() {
        if (peek() == '\\') {
            advance();
            return read_escape();
        }

        return advance();
    }

    unsigned char read_escape() {
        if (is_at_end()) {
            fail("unterminated escape sequence");
        }

        const unsigned char ch = advance();
        switch (ch) {
        case '\'':
            return '\'';
        case '"':
            return '"';
        case '?':
            return '?';
        case '\\':
            return '\\';
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case 'v':
            return '\v';
        case 'x':
            return read_hex_escape();
        default:
            if (is_oct_digit(ch)) {
                return read_octal_escape(ch);
            }
            fail("unknown escape sequence");
        }
    }

    unsigned char read_hex_escape() {
        if (!is_hex_digit(peek())) {
            fail("hex escape sequence requires at least one digit");
        }

        std::uint64_t value = 0;
        while (is_hex_digit(peek())) {
            value = checked_append_digit(value, 16, hex_value(advance()));
            if (value > std::numeric_limits<unsigned char>::max()) {
                fail("escape sequence does not fit in a byte");
            }
        }

        return static_cast<unsigned char>(value);
    }

    unsigned char read_octal_escape(unsigned char first) {
        std::uint64_t value = static_cast<std::uint64_t>(first - '0');

        for (int i = 0; i < 2 && is_oct_digit(peek()); ++i) {
            value = checked_append_digit(value, 8, advance() - '0');
            if (value > std::numeric_limits<unsigned char>::max()) {
                fail("escape sequence does not fit in a byte");
            }
        }

        return static_cast<unsigned char>(value);
    }

    std::uint64_t checked_append_digit(std::uint64_t value, int base, int digit) {
        constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();

        if (value > (max - static_cast<std::uint64_t>(digit)) / static_cast<std::uint64_t>(base)) {
            fail("integer literal is too large");
        }

        return value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
    }

    void add_token(
        TokenType type,
        std::size_t start,
        std::size_t start_line,
        std::size_t start_column,
        std::uint64_t integer_value = 0,
        std::string text = ""
    ) {
        tokens.push_back(
            Token{
                type,
                std::string(source.substr(start, current - start)),
                start_line,
                start_column,
                integer_value,
                std::move(text)
            }
        );
    }

    [[noreturn]] void fail(std::string message) const {
        fail_at(line, column, std::move(message));
    }

    [[noreturn]] void fail_at(std::size_t error_line, std::size_t error_column, std::string message) const {
        throw std::runtime_error(
            "line " + std::to_string(error_line) + ", column " + std::to_string(error_column) + ": " + message
        );
    }
};

} // namespace

Lexer::Lexer(std::string source)
    : source(std::move(source)) {}

Lexer::Lexer(std::vector<unsigned char> bytes)
    : source(bytes.begin(), bytes.end()) {}

std::vector<Token> Lexer::lex() const {
    Scanner scanner(source);
    return scanner.lex();
}

std::string_view token_type_name(TokenType type) noexcept {
    switch (type) {
    case TokenType::LeftParen:
        return "LeftParen";
    case TokenType::RightParen:
        return "RightParen";
    case TokenType::LeftBrace:
        return "LeftBrace";
    case TokenType::RightBrace:
        return "RightBrace";
    case TokenType::LeftBracket:
        return "LeftBracket";
    case TokenType::RightBracket:
        return "RightBracket";
    case TokenType::Semicolon:
        return "Semicolon";
    case TokenType::Comma:
        return "Comma";
    case TokenType::Dot:
        return "Dot";
    case TokenType::Colon:
        return "Colon";
    case TokenType::Question:
        return "Question";
    case TokenType::Plus:
        return "Plus";
    case TokenType::Minus:
        return "Minus";
    case TokenType::Star:
        return "Star";
    case TokenType::Slash:
        return "Slash";
    case TokenType::Percent:
        return "Percent";
    case TokenType::PlusPlus:
        return "PlusPlus";
    case TokenType::MinusMinus:
        return "MinusMinus";
    case TokenType::Equal:
        return "Equal";
    case TokenType::PlusEqual:
        return "PlusEqual";
    case TokenType::MinusEqual:
        return "MinusEqual";
    case TokenType::StarEqual:
        return "StarEqual";
    case TokenType::SlashEqual:
        return "SlashEqual";
    case TokenType::PercentEqual:
        return "PercentEqual";
    case TokenType::EqualEqual:
        return "EqualEqual";
    case TokenType::BangEqual:
        return "BangEqual";
    case TokenType::Less:
        return "Less";
    case TokenType::LessEqual:
        return "LessEqual";
    case TokenType::Greater:
        return "Greater";
    case TokenType::GreaterEqual:
        return "GreaterEqual";
    case TokenType::Bang:
        return "Bang";
    case TokenType::Tilde:
        return "Tilde";
    case TokenType::Ampersand:
        return "Ampersand";
    case TokenType::Pipe:
        return "Pipe";
    case TokenType::Caret:
        return "Caret";
    case TokenType::AmpersandAmpersand:
        return "AmpersandAmpersand";
    case TokenType::PipePipe:
        return "PipePipe";
    case TokenType::LessLess:
        return "LessLess";
    case TokenType::GreaterGreater:
        return "GreaterGreater";
    case TokenType::AmpersandEqual:
        return "AmpersandEqual";
    case TokenType::PipeEqual:
        return "PipeEqual";
    case TokenType::CaretEqual:
        return "CaretEqual";
    case TokenType::LessLessEqual:
        return "LessLessEqual";
    case TokenType::GreaterGreaterEqual:
        return "GreaterGreaterEqual";
    case TokenType::Arrow:
        return "Arrow";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::IntegerLiteral:
        return "IntegerLiteral";
    case TokenType::CharacterLiteral:
        return "CharacterLiteral";
    case TokenType::StringLiteral:
        return "StringLiteral";
    case TokenType::KeywordIf:
        return "KeywordIf";
    case TokenType::KeywordElse:
        return "KeywordElse";
    case TokenType::KeywordWhile:
        return "KeywordWhile";
    case TokenType::KeywordFor:
        return "KeywordFor";
    case TokenType::KeywordBreak:
        return "KeywordBreak";
    case TokenType::KeywordContinue:
        return "KeywordContinue";
    case TokenType::KeywordReturn:
        return "KeywordReturn";
    case TokenType::KeywordSwitch:
        return "KeywordSwitch";
    case TokenType::KeywordCase:
        return "KeywordCase";
    case TokenType::KeywordDefault:
        return "KeywordDefault";
    case TokenType::KeywordStruct:
        return "KeywordStruct";
    case TokenType::KeywordEnum:
        return "KeywordEnum";
    case TokenType::KeywordStatic:
        return "KeywordStatic";
    case TokenType::KeywordTrue:
        return "KeywordTrue";
    case TokenType::KeywordFalse:
        return "KeywordFalse";
    case TokenType::KeywordBool:
        return "KeywordBool";
    case TokenType::KeywordVoid:
        return "KeywordVoid";
    case TokenType::KeywordChar:
        return "KeywordChar";
    case TokenType::KeywordShort:
        return "KeywordShort";
    case TokenType::KeywordInt:
        return "KeywordInt";
    case TokenType::KeywordLong:
        return "KeywordLong";
    case TokenType::KeywordSigned:
        return "KeywordSigned";
    case TokenType::KeywordUnsigned:
        return "KeywordUnsigned";
    case TokenType::KeywordInt8:
        return "KeywordInt8";
    case TokenType::KeywordInt16:
        return "KeywordInt16";
    case TokenType::KeywordInt32:
        return "KeywordInt32";
    case TokenType::KeywordInt64:
        return "KeywordInt64";
    case TokenType::KeywordUInt8:
        return "KeywordUInt8";
    case TokenType::KeywordUInt16:
        return "KeywordUInt16";
    case TokenType::KeywordUInt32:
        return "KeywordUInt32";
    case TokenType::KeywordUInt64:
        return "KeywordUInt64";
    case TokenType::EndOfFile:
        return "EndOfFile";
    }

    return "Unknown";
}

} // namespace zenith::compiler
