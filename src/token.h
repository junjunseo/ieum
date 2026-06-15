#ifndef IEUM_TOKEN_H
#define IEUM_TOKEN_H

#include <string>

// ── 토큰 타입 ──────────────────────────────────────────
// 1학기 범위: 구조 선언만 다룬다. 로직(제어/연산)은 2학기로 미룸.
enum class TokenType {
    // 구조 선언 키워드
    MODULE,     // module  — 모듈 선언
    DEPENDS,    // depends — 의존 선언
    LAYER,      // layer   — 계층 선언
    ABOVE,      // above   — 계층 상하 관계

    // 기타
    IDENTIFIER, // 모듈/계층 이름
    COMMA,      // ,  — 의존 대상 나열
    NEWLINE,    // 줄바꿈 — 선언 구분
    END,        // 파일 끝
    UNKNOWN,    // 알 수 없음
};

// 토큰 타입 → 이름 문자열
inline std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::MODULE:     return "키워드(module)";
        case TokenType::DEPENDS:    return "키워드(depends)";
        case TokenType::LAYER:      return "키워드(layer)";
        case TokenType::ABOVE:      return "키워드(above)";
        case TokenType::IDENTIFIER: return "식별자";
        case TokenType::COMMA:      return "쉼표(,)";
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