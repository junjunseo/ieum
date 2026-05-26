#ifndef IEUM_TOKEN_H
#define IEUM_TOKEN_H

#include <string>

// ── 토큰 타입 ──────────────────────────────────────────
enum class TokenType {
    // 키워드
    MANYAK,     // 만약
    IMYEON,     // 이면
    ANIMYEON,   // 아니면
    BABOK,      // 반복
    DONGAHN,    // 동안
    BEON,       // 번
    CHULRYUK,   // 출력
    HAMSU,      // 함수, 추후 확장용

    // 리터럴
    NUMBER,     // 숫자
    STRING,     // 문자열
    BOOL_TRUE,  // 참
    BOOL_FALSE, // 거짓

    // 연산자
    ASSIGN,     // =
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    EQ,         // ==
    NEQ,        // !=
    LT,         // <
    GT,         // >
    LTE,        // <=
    GTE,        // >=

    // 기타
    IDENTIFIER, // 변수명
    LPAREN,     // (
    RPAREN,     // )
    COLON,      // :
    NEWLINE,    // 줄바꿈
    END,        // 파일 끝
    UNKNOWN,    // 알 수 없음
};

// 토큰 타입 → 이름 문자열
inline std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::MANYAK:     return "키워드(만약)";
        case TokenType::IMYEON:     return "키워드(이면)";
        case TokenType::ANIMYEON:   return "키워드(아니면)";
        case TokenType::BABOK:      return "키워드(반복)";
        case TokenType::DONGAHN:    return "키워드(동안)";
        case TokenType::BEON:       return "키워드(번)";
        case TokenType::CHULRYUK:   return "키워드(출력)";
        case TokenType::NUMBER:     return "숫자";
        case TokenType::STRING:     return "문자열";
        case TokenType::BOOL_TRUE:  return "불리언(참)";
        case TokenType::BOOL_FALSE: return "불리언(거짓)";
        case TokenType::ASSIGN:     return "연산자(=)";
        case TokenType::PLUS:       return "연산자(+)";
        case TokenType::MINUS:      return "연산자(-)";
        case TokenType::STAR:       return "연산자(*)";
        case TokenType::SLASH:      return "연산자(/)";
        case TokenType::EQ:         return "연산자(==)";
        case TokenType::NEQ:        return "연산자(!=)";
        case TokenType::LT:         return "연산자(<)";
        case TokenType::GT:         return "연산자(>)";
        case TokenType::LTE:        return "연산자(<=)";
        case TokenType::GTE:        return "연산자(>=)";
        case TokenType::IDENTIFIER: return "식별자";
        case TokenType::LPAREN:     return "괄호(()";
        case TokenType::RPAREN:     return "괄호())";
        case TokenType::COLON:      return "콜론(:)";
        case TokenType::NEWLINE:    return "줄바꿈";
        case TokenType::END:        return "끝";
        default:                    return "알 수 없음";
    }
}

// ── 토큰 구조체 ────────────────────────────────────────
struct Token {
    TokenType type;
    std::string value;
    int line;

    Token(TokenType t, std::string v, int l)
        : type(t), value(v), line(l) {}
};

#endif // IEUM_TOKEN_H
