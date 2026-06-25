#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "token.h"
#include "ast.h"
#include "parser.h"

namespace {

int passed = 0;
int failed = 0;

void expect(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[통과] " << name << "\n";
        passed++;
    } else {
        std::cerr << "[실패] " << name << "\n";
        failed++;
    }
}

void expectThrows(const std::vector<Token>& tokens, const std::string& name) {
    try {
        Parser parser(tokens);
        parser.parse();
        expect(false, name);
    } catch (const std::runtime_error&) {
        expect(true, name);
    }
}

} // namespace

int main() {
    const std::vector<Token> validTokens = {
        {TokenType::MODULE,     "module",  1}, {TokenType::IDENTIFIER, "domain", 1}, {TokenType::NEWLINE, "", 1},
        {TokenType::MODULE,     "module",  2}, {TokenType::IDENTIFIER, "ui",     2}, {TokenType::DEPENDS, "depends", 2}, {TokenType::IDENTIFIER, "domain", 2}, {TokenType::NEWLINE, "", 2},
        {TokenType::MODULE,     "module",  3}, {TokenType::IDENTIFIER, "infra",  3}, {TokenType::DEPENDS, "depends", 3}, {TokenType::IDENTIFIER, "domain", 3}, {TokenType::COMMA, ",", 3}, {TokenType::IDENTIFIER, "ui", 3}, {TokenType::NEWLINE, "", 3},
        {TokenType::LAYER,      "layer",   4}, {TokenType::IDENTIFIER, "ui",     4}, {TokenType::ABOVE, "above", 4}, {TokenType::IDENTIFIER, "domain", 4}, {TokenType::NEWLINE, "", 4},
        {TokenType::END,        "",        5},
    };

    try {
        Parser parser(validTokens);
        Program prog = parser.parse();
        expect(prog.modules.size() == 3, "모듈 선언 3개 파싱");
        expect(prog.modules[1].name == "ui", "모듈 이름 보존");
        expect(prog.modules[2].deps.size() == 2, "복수 의존 대상 파싱");
        expect(prog.modules[2].deps[1] == "ui", "의존 대상 순서 보존");
        expect(prog.layers.size() == 1, "계층 선언 파싱");
        expect(prog.layers[0].upper == "ui" &&
               prog.layers[0].lower == "domain", "계층 상하 관계 보존");
    } catch (const std::exception& e) {
        std::cerr << "[실패] 정상 토큰 파싱 중 예외: " << e.what() << "\n";
        failed++;
    }

    expectThrows({
        {TokenType::MODULE, "module", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "모듈 이름 누락 거부");

    expectThrows({
        {TokenType::LAYER, "layer", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::IDENTIFIER, "domain", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "above 없는 계층 선언 거부");

    expectThrows({
        {TokenType::MODULE, "module", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::DEPENDS, "depends", 1},
        {TokenType::IDENTIFIER, "domain", 1},
        {TokenType::MODULE, "module", 1},
        {TokenType::IDENTIFIER, "extra", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "한 줄의 복수 선언 거부");

    std::cout << "\n파서 테스트: " << passed << "개 통과, "
              << failed << "개 실패\n";
    return failed == 0 ? 0 : 1;
}
