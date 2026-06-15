#ifndef IEUM_LEXER_H
#define IEUM_LEXER_H

#include <string>
#include <vector>
#include <cctype>
#include "token.h"

// ── 렉서 ───────────────────────────────────────────────
// 소스 문자열을 토큰 시퀀스로 변환한다.
// 1학기 범위: 구조 선언만 다루므로 인식 대상은
//   키워드(module/depends/layer/above), 식별자, 쉼표, 줄바꿈 뿐이다.
// 식별자 규칙: [A-Za-z_][A-Za-z0-9_]* (영어 전용).
class Lexer {
public:
    Lexer(const std::string& source) : src(source), pos(0), line(1) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;

        while (pos < src.size()) {
            skipWhitespace();
            if (pos >= src.size()) break;

            // 줄바꿈 — 선언 구분자
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

            // 쉼표 — 의존 대상 나열
            if (src[pos] == ',') {
                tokens.push_back(Token(TokenType::COMMA, ",", line));
                pos++;
                continue;
            }

            // 단어 (키워드 or 식별자)
            if (isIdentStart(pos)) {
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
    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
            pos++;
    }

    // 식별자 첫 글자: 영문자 또는 밑줄
    bool isIdentStart(size_t p) {
        unsigned char c = src[p];
        return std::isalpha(c) || c == '_';
    }

    // 식별자 이후 글자: 영문자, 숫자, 밑줄
    bool isIdentPart(size_t p) {
        unsigned char c = src[p];
        return std::isalnum(c) || c == '_';
    }

    // ── 단어(키워드 or 식별자) 읽기 ──────────────────
    Token readWord() {
        size_t start = pos;
        while (pos < src.size() && isIdentPart(pos))
            pos++;
        std::string word = src.substr(start, pos - start);

        // 키워드 매핑 (구조 선언 전용)
        if (word == "module")  return Token(TokenType::MODULE,  word, line);
        if (word == "depends") return Token(TokenType::DEPENDS, word, line);
        if (word == "layer")   return Token(TokenType::LAYER,   word, line);
        if (word == "above")   return Token(TokenType::ABOVE,   word, line);

        return Token(TokenType::IDENTIFIER, word, line);
    }
};

#endif // IEUM_LEXER_H