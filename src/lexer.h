#ifndef IEUM_LEXER_H
#define IEUM_LEXER_H

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "token.h"

class Lexer {
public:
    explicit Lexer(const std::string& source, std::string file = {})
        : src(source), file_(std::move(file)) {}

    std::vector<Token> tokenize() {
        pos = 0; line = 1; column = 1;
        if (src.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3;
        std::vector<Token> tokens;
        while (pos < src.size()) {
            const char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
            if (c == '#') {
                while (pos < src.size() && src[pos] != '\n') advance();
                continue;
            }
            const auto start = location();
            if (c == '\n') {
                advance(); tokens.push_back(make(TokenType::NEWLINE, "\\n", start));
            } else if (identStart(c)) {
                const auto begin = pos;
                while (pos < src.size() && (identStart(src[pos]) || digit(src[pos]))) advance();
                const auto word = src.substr(begin, pos - begin);
                TokenType type = TokenType::IDENTIFIER;
                if (word == "module") type = TokenType::MODULE;
                else if (word == "depends") type = TokenType::DEPENDS;
                else if (word == "layer") type = TokenType::LAYER;
                else if (word == "above") type = TokenType::ABOVE;
                else if (word == "fn") type = TokenType::FN;
                else if (word == "let") type = TokenType::LET;
                else if (word == "call") type = TokenType::CALL;
                else if (word == "true") type = TokenType::TRUE_VALUE;
                else if (word == "false") type = TokenType::FALSE_VALUE;
                tokens.push_back(make(type, word, start));
            } else if (digit(c)) {
                const auto begin = pos;
                while (pos < src.size() && digit(src[pos])) advance();
                tokens.push_back(make(TokenType::INTEGER, src.substr(begin, pos - begin), start));
            } else if (c == '"') {
                tokens.push_back(readString(start));
            } else {
                const auto begin = pos;
                advance();
                TokenType type = TokenType::UNKNOWN;
                switch (c) {
                    case '{': type = TokenType::LEFT_BRACE; break;
                    case '}': type = TokenType::RIGHT_BRACE; break;
                    case '(': type = TokenType::LEFT_PAREN; break;
                    case ')': type = TokenType::RIGHT_PAREN; break;
                    case ',': type = TokenType::COMMA; break;
                    case ':': type = TokenType::COLON; break;
                    case '+': type = TokenType::PLUS; break;
                    case '-': type = TokenType::MINUS; break;
                    case '*': type = TokenType::STAR; break;
                    case '/': type = TokenType::SLASH; break;
                    case '%': type = TokenType::PERCENT; break;
                    case '=': type = match('=') ? TokenType::EQUAL : TokenType::ASSIGN; break;
                    case '!': type = match('=') ? TokenType::NOT_EQUAL : TokenType::BANG; break;
                    case '<': type = match('=') ? TokenType::LESS_EQUAL : TokenType::LESS; break;
                    case '>': type = match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER; break;
                    case '&': if (match('&')) type = TokenType::AND; break;
                    case '|': if (match('|')) type = TokenType::OR; break;
                }
                tokens.push_back(make(type, src.substr(begin, pos - begin), start));
            }
        }
        tokens.push_back(make(TokenType::END, "", location()));
        return tokens;
    }

private:
    std::string src, file_;
    std::size_t pos = 0;
    int line = 1, column = 1;
    static bool identStart(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }
    static bool digit(char c) { return c >= '0' && c <= '9'; }
    SourceSpan location() const { return {file_, line, column, line, column}; }
    void advance() {
        if (src[pos++] == '\n') { ++line; column = 1; }
        else ++column;
    }
    bool match(char c) {
        if (pos >= src.size() || src[pos] != c) return false;
        advance(); return true;
    }
    Token make(TokenType type, std::string value, SourceSpan start) const {
        start.endLine = line; start.endColumn = column;
        return Token(type, std::move(value), start.line, start);
    }
    [[noreturn]] void fail(const SourceSpan& span, const std::string& message) const {
        throw std::runtime_error(sourceLocation(span) + " 렉싱 오류: " + message);
    }
    static bool validUtf8(const std::string& value) {
        for (std::size_t i = 0; i < value.size();) {
            const auto c = static_cast<unsigned char>(value[i++]);
            if (c < 0x80) continue;
            unsigned code;
            int count;
            unsigned minimum;
            if (c >= 0xC2 && c <= 0xDF) { code = c & 0x1F; count = 1; minimum = 0x80; }
            else if (c >= 0xE0 && c <= 0xEF) { code = c & 0x0F; count = 2; minimum = 0x800; }
            else if (c >= 0xF0 && c <= 0xF4) { code = c & 7; count = 3; minimum = 0x10000; }
            else return false;
            while (count--) {
                if (i == value.size()) return false;
                const auto next = static_cast<unsigned char>(value[i++]);
                if ((next & 0xC0) != 0x80) return false;
                code = (code << 6) | (next & 0x3F);
            }
            if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
        }
        return true;
    }
    Token readString(const SourceSpan& start) {
        advance();
        std::string value;
        while (pos < src.size()) {
            const auto here = location();
            char c = src[pos];
            if (c == '"') {
                advance();
                if (!validUtf8(value)) fail(start, "유효한 UTF-8 문자열이 필요합니다");
                return make(TokenType::STRING, value, start);
            }
            if (static_cast<unsigned char>(c) < 0x20) fail(here, "문자열의 제어 문자는 escape로 작성해야 합니다");
            advance();
            if (c == '\\') {
                if (pos == src.size()) fail(start, "문자열을 닫는 따옴표가 필요합니다");
                c = src[pos]; advance();
                switch (c) {
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case '\\': case '"': break;
                    default: fail(here, "지원하지 않는 문자열 escape입니다");
                }
            }
            value += c;
        }
        fail(start, "문자열을 닫는 따옴표가 필요합니다");
    }
};

#endif
