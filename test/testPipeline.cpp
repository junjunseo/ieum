#include <iostream>
#include <stdexcept>
#include <string>
#include "token.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"

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

Program parse(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

} // namespace

int main() {
    const std::string validSource =
        "# 주석과 빈 줄도 처리한다\n"
        "\n"
        "module domain\n"
        "module service depends domain\n"
        "module ui depends service\n"
        "layer ui above service\n"
        "layer service above domain\n";

    try {
        Program prog = parse(validSource);
        expect(prog.modules.size() == 3, "Lexer → Parser 모듈 변환");
        expect(prog.layers.size() == 2, "Lexer → Parser 계층 변환");

        Checker checker(prog);
        expect(checker.check().empty(), "정상 구조 전체 파이프라인 통과");
    } catch (const std::exception& e) {
        std::cerr << "[실패] 정상 파이프라인 예외: " << e.what() << "\n";
        failed++;
    }

    try {
        Program prog = parse(
            "\xEF\xBB\xBF"
            "module data\n"
            "module service depends data\n");
        expect(prog.modules.size() == 2 &&
               prog.modules[0].name == "data" &&
               prog.modules[1].deps.size() == 1,
               "UTF-8 BOM 입력 파싱");
    } catch (const std::exception& e) {
        std::cerr << "[실패] BOM 입력 예외: " << e.what() << "\n";
        failed++;
    }

    try {
        parse("module ui @ domain\n");
        expect(false, "알 수 없는 문자 포함 문법 거부");
    } catch (const std::runtime_error&) {
        expect(true, "알 수 없는 문자 포함 문법 거부");
    }

    try {
        parse("module ui depends\n");
        expect(false, "불완전한 의존 선언 거부");
    } catch (const std::runtime_error&) {
        expect(true, "불완전한 의존 선언 거부");
    }

    std::cout << "\n통합 테스트: " << passed << "개 통과, "
              << failed << "개 실패\n";
    return failed == 0 ? 0 : 1;
}
