#include <iostream>
#include <string>
#include <vector>
#include "lexer.h"

// ── 이음(Ieum) 언어 - 엔트리포인트 ─────────────────────
// 현재 단계: 렉서까지 구현. 입력 소스를 토큰으로 분해해 출력한다.
int main() {
    // 테스트용 이음 코드
    std::string code = R"(
점수 = 95
만약 점수 >= 90 이면:
    출력("잘했어요!")
아니면:
    출력("다시 해봐요")

반복 3 번:
    출력("안녕하세요")

횟수 = 0
반복 횟수 < 5 동안:
    출력(횟수)
    횟수 = 횟수 + 1
)";

    std::cout << "=== 입력 코드 ===" << std::endl;
    std::cout << code << std::endl;
    std::cout << "=== 토큰 목록 ===" << std::endl;

    Lexer lexer(code);
    std::vector<Token> tokens = lexer.tokenize();

    for (const Token& tok : tokens) {
        if (tok.type == TokenType::NEWLINE) continue; // 출력 깔끔하게
        if (tok.type == TokenType::END) break;

        std::cout << "[" << tok.line << "번 줄] "
                  << tokenTypeName(tok.type);
        if (!tok.value.empty())
            std::cout << " -> \"" << tok.value << "\"";
        std::cout << std::endl;
    }

    return 0;
}
