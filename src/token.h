#ifndef IEUM_TOKEN_H
#define IEUM_TOKEN_H

#include <string>
#include <utility>
#include "source.h"

// ── 토큰 타입 ──────────────────────────────────────────
// 구조 선언과 F1 모듈 본문 문법을 다룬다.
enum class TokenType {
    // 구조 선언 키워드
    MODULE,     // module  — 모듈 선언
    DEPENDS,    // depends — 의존 선언
    LAYER,      // layer   — 계층 선언
    ABOVE,      // above   — 계층 상하 관계

    // 모듈 본문 키워드
    FN,         // fn      — 함수 선언
    LET,        // let     — 변수 선언
    CALL,       // call    — 함수 호출
    INTEGER, STRING, TRUE_VALUE, FALSE_VALUE,
    COLON, ASSIGN, PLUS, MINUS, STAR, SLASH, PERCENT,
    BANG, EQUAL, NOT_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    AND, OR,
    RETURN, IF, ELSE, WHILE, BREAK, CONTINUE, ARROW,

    // 기타
    IDENTIFIER, // 모듈/계층/변수/함수 이름
    LEFT_BRACE, // {
    RIGHT_BRACE, // }
    LEFT_PAREN, // (
    RIGHT_PAREN, // )
    COMMA,      // ,  — 이름/인자 나열
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
        case TokenType::FN:         return "키워드(fn)";
        case TokenType::LET:        return "키워드(let)";
        case TokenType::CALL:       return "키워드(call)";
        case TokenType::INTEGER:    return "정수";
        case TokenType::STRING:     return "문자열";
        case TokenType::IDENTIFIER: return "식별자";
        case TokenType::LEFT_BRACE: return "왼쪽 중괄호({)";
        case TokenType::RIGHT_BRACE: return "오른쪽 중괄호(})";
        case TokenType::LEFT_PAREN: return "왼쪽 소괄호('(')";
        case TokenType::RIGHT_PAREN: return "오른쪽 소괄호(')')";
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
    SourceSpan span;

    Token(TokenType t, std::string v, int l, SourceSpan s = {})
        : type(t), value(v), line(l), span(std::move(s)) { span.line = l; }
};

#endif // IEUM_TOKEN_H
