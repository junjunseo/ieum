#include <iostream>
#include <vector>
#include "token.h"
#include "ast.h"
#include "parser.h"

// 파싱된 구조를 사람이 읽게 출력
void dump(const Program& prog) {
    std::cout << "── 모듈 (" << prog.modules.size() << ") ──\n";
    for (const auto& m : prog.modules) {
        std::cout << "  module " << m.name;
        if (!m.deps.empty()) {
            std::cout << " depends ";
            for (size_t i = 0; i < m.deps.size(); i++) {
                std::cout << m.deps[i];
                if (i + 1 < m.deps.size()) std::cout << ", ";
            }
        }
        std::cout << "  (line " << m.line << ")\n";
    }
    std::cout << "── 계층 (" << prog.layers.size() << ") ──\n";
    for (const auto& l : prog.layers) {
        std::cout << "  layer " << l.upper << " above " << l.lower
                  << "  (line " << l.line << ")\n";
    }
}

int main() {
    // 실제로는 Lexer가 만들어 줄 토큰 스트림.
    // 여기서는 파서만 단독 검증하려고 손으로 구성한다.
    //   module domain
    //   module ui depends domain
    //   module infra depends domain, ui
    //   layer ui above domain
    std::vector<Token> tokens = {
        {TokenType::MODULE,     "module",  1}, {TokenType::IDENTIFIER, "domain", 1}, {TokenType::NEWLINE, "", 1},
        {TokenType::MODULE,     "module",  2}, {TokenType::IDENTIFIER, "ui",     2}, {TokenType::DEPENDS, "depends", 2}, {TokenType::IDENTIFIER, "domain", 2}, {TokenType::NEWLINE, "", 2},
        {TokenType::MODULE,     "module",  3}, {TokenType::IDENTIFIER, "infra",  3}, {TokenType::DEPENDS, "depends", 3}, {TokenType::IDENTIFIER, "domain", 3}, {TokenType::COMMA, ",", 3}, {TokenType::IDENTIFIER, "ui", 3}, {TokenType::NEWLINE, "", 3},
        {TokenType::LAYER,      "layer",   4}, {TokenType::IDENTIFIER, "ui",     4}, {TokenType::ABOVE, "above", 4}, {TokenType::IDENTIFIER, "domain", 4}, {TokenType::NEWLINE, "", 4},
        {TokenType::END,        "",        5},
    };

    try {
        Parser parser(tokens);
        Program prog = parser.parse();
        dump(prog);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}