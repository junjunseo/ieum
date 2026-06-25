#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "token.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"

// 위반 종류 → 사람이 읽는 라벨
static std::string kindLabel(Violation::Kind k) {
    switch (k) {
        case Violation::Kind::DuplicateModule:      return "중복 모듈";
        case Violation::Kind::UndefinedLayerModule: return "미선언 계층 모듈";
        case Violation::Kind::SelfLayer:            return "자기 계층";
        case Violation::Kind::ImplicitDependency: return "암묵적 의존";
        case Violation::Kind::CyclicDependency:   return "순환 의존";
        case Violation::Kind::LayerViolation:     return "계층 위반";
    }
    return "알 수 없음";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "사용법: ieum <소스파일.ieum>\n";
        return 2;
    }

    // 1) 파일 읽기
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "오류: 파일을 열 수 없습니다 — " << argv[1] << "\n";
        return 2;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try {
        // 2) 렉싱 → 파싱
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        Program prog = parser.parse();

        // 3) 구조 요약 출력
        std::cout << "── 파싱 결과 ──\n";
        std::cout << "모듈 " << prog.modules.size()
                  << "개, 계층 선언 " << prog.layers.size() << "개\n\n";

        // 4) 의존 검사 (끌 수 없음 — 항상 전부 적용)
        Checker checker(prog);
        auto violations = checker.check();

        if (violations.empty()) {
            std::cout << "✓ 구조 검사 통과: 위반 없음\n";
            return 0;
        }

        std::cout << "✗ 구조 검사 실패: 위반 " << violations.size() << "건\n\n";
        for (const auto& v : violations) {
            std::cout << "  [" << kindLabel(v.kind) << "] "
                      << v.message;
            if (v.line > 0) std::cout << " (" << v.line << "행)";
            std::cout << "\n";
        }
        // 위반 시 비-0 종료: 빌드/CI에서 실패로 드러난다
        return 1;

    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
