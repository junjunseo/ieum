#ifndef IEUM_PARSER_H
#define IEUM_PARSER_H

#include <string>
#include <vector>
#include <stdexcept>
#include "token.h"
#include "ast.h"

// ── 파서 ───────────────────────────────────────────────
// 토큰 스트림(렉서 출력)을 받아 Program(구조 AST)을 만든다.
// 문법(1학기):
//   program     := { declaration }
//   declaration := moduleDecl | layerDecl | NEWLINE
//   moduleDecl  := MODULE IDENTIFIER [ DEPENDS identList ]
//   layerDecl   := LAYER IDENTIFIER ABOVE IDENTIFIER
//   identList   := IDENTIFIER { COMMA IDENTIFIER }
class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens)) {}

    Program parse() {
        Program prog;
        while (!isAtEnd()) {
            // 빈 줄은 건너뛴다
            if (check(TokenType::NEWLINE)) { advance(); continue; }

            if (check(TokenType::MODULE)) {
                prog.modules.push_back(parseModule());
            } else if (check(TokenType::LAYER)) {
                prog.layers.push_back(parseLayer());
            } else {
                throw error("선언은 'module' 또는 'layer'로 시작해야 합니다");
            }
            consumeLineEnd();
        }
        return prog;
    }

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    // ── 선언 파싱 ──────────────────────────────────────
    ModuleDecl parseModule() {
        Token kw = advance();                 // MODULE
        Token name = expect(TokenType::IDENTIFIER,
                            "module 다음에는 모듈 이름이 와야 합니다");
        ModuleDecl decl;
        decl.name = name.value;
        decl.line = kw.line;

        if (check(TokenType::DEPENDS)) {
            advance();                        // DEPENDS
            decl.deps.push_back(
                expect(TokenType::IDENTIFIER,
                       "depends 다음에는 의존 대상 이름이 와야 합니다").value);
            while (check(TokenType::COMMA)) {
                advance();                    // COMMA
                decl.deps.push_back(
                    expect(TokenType::IDENTIFIER,
                           "',' 다음에는 의존 대상 이름이 와야 합니다").value);
            }
        }
        return decl;
    }

    LayerDecl parseLayer() {
        Token kw = advance();                 // LAYER
        Token upper = expect(TokenType::IDENTIFIER,
                             "layer 다음에는 계층 이름이 와야 합니다");
        expect(TokenType::ABOVE, "계층 선언에는 'above'가 필요합니다");
        Token lower = expect(TokenType::IDENTIFIER,
                             "above 다음에는 하위 계층 이름이 와야 합니다");
        LayerDecl decl;
        decl.upper = upper.value;
        decl.lower = lower.value;
        decl.line  = kw.line;
        return decl;
    }

    // ── 토큰 유틸 ──────────────────────────────────────
    bool isAtEnd() const { return peek().type == TokenType::END; }
    const Token& peek() const { return tokens_[pos_]; }
    bool check(TokenType t) const { return peek().type == t; }

    Token advance() {
        Token t = tokens_[pos_];
        if (!isAtEnd()) pos_++;
        return t;
    }

    Token expect(TokenType t, const std::string& msg) {
        if (check(t)) return advance();
        throw error(msg);
    }

    // 선언 끝: NEWLINE 또는 파일 끝
    void consumeLineEnd() {
        if (check(TokenType::NEWLINE)) { advance(); return; }
        if (isAtEnd()) return;
        throw error("한 줄에는 하나의 선언만 올 수 있습니다");
    }

    std::runtime_error error(const std::string& msg) const {
        return std::runtime_error(
            "[" + std::to_string(peek().line) + "행] 파싱 오류: " + msg +
            " (현재 토큰: " + tokenTypeName(peek().type) +
            (peek().value.empty() ? "" : " '" + peek().value + "'") + ")");
    }
};

#endif // IEUM_PARSER_H