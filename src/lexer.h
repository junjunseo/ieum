#ifndef IEUM_LEXER_H
#define IEUM_LEXER_H

#include <string>
#include <vector>
#include <cctype>
#include "token.h"

// ── 렉서 ───────────────────────────────────────────────
// 소스 문자열을 토큰 시퀀스로 변환한다.
// 한글은 UTF-8에서 3바이트로 표현되므로, 바이트 단위가 아닌
// 문자(rune) 단위로 식별자/키워드를 읽는다.
class Lexer {
public:
    Lexer(const std::string& source) : src(source), pos(0), line(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (pos < src.size()) {
            skipWhitespace();
            if (pos >= src.size()) break;

            // 줄바꿈
            if (src[pos] == '\n') {
                tokens.push_back(Token(TokenType::NEWLINE, "\\n", line));
                line++;
                pos++;
                continue;
            }

            // 주석 (#으로 시작하면 줄 끝까지 무시)
            if (src[pos] == '#') {
                while (pos < src.size() && src[pos] != '\n') pos++;
                continue;
            }

            // 문자열 리터럴
            if (src[pos] == '"') {
                tokens.push_back(readString());
                continue;
            }

            // 숫자
            if (isDigit(src[pos])) {
                tokens.push_back(readNumber());
                continue;
            }

            // 연산자 / 기호
            Token opToken = tryReadOperator();
            if (opToken.type != TokenType::UNKNOWN) {
                tokens.push_back(opToken);
                continue;
            }

            // 한글 식별자 or 키워드
            if (isHangulOrAlpha(pos)) {
                tokens.push_back(readWord());
                continue;
            }

            // 알 수 없는 문자
            tokens.push_back(Token(TokenType::UNKNOWN, std::string(1, src[pos]), line));
            pos++;
        }

        tokens.push_back(Token(TokenType::END, "", line));
        return tokens;
    }

private:
    std::string src;
    size_t pos;
    int line;

    // ── 헬퍼 ─────────────────────────────────────────
    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
            pos++;
    }

    // UTF-8에서 한글은 3바이트 (0xEA~0xED로 시작)
    bool isHangulStart(size_t p) {
        if (p + 2 >= src.size()) return false;
        unsigned char c = src[p];
        return (c >= 0xEA && c <= 0xED);
    }

    bool isHangulOrAlpha(size_t p) {
        return isHangulStart(p) || isalpha(static_cast<unsigned char>(src[p])) || src[p] == '_';
    }

    // 다음 한글 or 알파벳 문자(rune) 읽기
    std::string readRune() {
        if (isHangulStart(pos)) {
            std::string ch = src.substr(pos, 3);
            pos += 3;
            return ch;
        }
        return std::string(1, src[pos++]);
    }

    // ── 숫자 읽기 ─────────────────────────────────────
    Token readNumber() {
        size_t start = pos;
        while (pos < src.size() && (isDigit(src[pos]) || src[pos] == '.'))
            pos++;
        return Token(TokenType::NUMBER, src.substr(start, pos - start), line);
    }

    // ── 문자열 읽기 ───────────────────────────────────
    Token readString() {
        pos++; // 여는 "
        std::string result;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                pos++;
                if (src[pos] == 'n') result += '\n';
                else result += src[pos];
                pos++;
            } else {
                result += src[pos++];
            }
        }
        if (pos < src.size()) pos++; // 닫는 "
        return Token(TokenType::STRING, result, line);
    }

    // ── 연산자 읽기 ───────────────────────────────────
    Token tryReadOperator() {
        char c = src[pos];
        char next = (pos + 1 < src.size()) ? src[pos + 1] : '\0';

        if (c == '=' && next == '=') { pos += 2; return Token(TokenType::EQ,     "==", line); }
        if (c == '!' && next == '=') { pos += 2; return Token(TokenType::NEQ,    "!=", line); }
        if (c == '<' && next == '=') { pos += 2; return Token(TokenType::LTE,    "<=", line); }
        if (c == '>' && next == '=') { pos += 2; return Token(TokenType::GTE,    ">=", line); }
        if (c == '=')  { pos++; return Token(TokenType::ASSIGN, "=",  line); }
        if (c == '+')  { pos++; return Token(TokenType::PLUS,   "+",  line); }
        if (c == '-')  { pos++; return Token(TokenType::MINUS,  "-",  line); }
        if (c == '*')  { pos++; return Token(TokenType::STAR,   "*",  line); }
        if (c == '/')  { pos++; return Token(TokenType::SLASH,  "/",  line); }
        if (c == '<')  { pos++; return Token(TokenType::LT,     "<",  line); }
        if (c == '>')  { pos++; return Token(TokenType::GT,     ">",  line); }
        if (c == '(')  { pos++; return Token(TokenType::LPAREN, "(",  line); }
        if (c == ')')  { pos++; return Token(TokenType::RPAREN, ")",  line); }
        if (c == ':')  { pos++; return Token(TokenType::COLON,  ":",  line); }

        return Token(TokenType::UNKNOWN, "", line);
    }

    // ── 단어(키워드 or 식별자) 읽기 ──────────────────
    Token readWord() {
        std::string word;
        while (pos < src.size() && isHangulOrAlpha(pos))
            word += readRune();

        // 키워드 매핑
        if (word == "만약")   return Token(TokenType::MANYAK,     word, line);
        if (word == "이면")   return Token(TokenType::IMYEON,     word, line);
        if (word == "아니면") return Token(TokenType::ANIMYEON,   word, line);
        if (word == "반복")   return Token(TokenType::BABOK,      word, line);
        if (word == "동안")   return Token(TokenType::DONGAHN,    word, line);
        if (word == "번")     return Token(TokenType::BEON,       word, line);
        if (word == "출력")   return Token(TokenType::CHULRYUK,   word, line);
        if (word == "참")     return Token(TokenType::BOOL_TRUE,  word, line);
        if (word == "거짓")   return Token(TokenType::BOOL_FALSE, word, line);

        return Token(TokenType::IDENTIFIER, word, line);
    }
};

#endif // IEUM_LEXER_H
