#include <iostream>
#include <string>
#include <vector>
#include "lexer.h"
#include "parser.h"
#include "checker.h"

namespace {

int passed = 0;
int failed = 0;

std::vector<Violation> check(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    Program prog = parser.parse();
    Checker checker(prog);
    return checker.check();
}

int countKind(const std::vector<Violation>& violations, Violation::Kind kind) {
    int count = 0;
    for (const auto& violation : violations) {
        if (violation.kind == kind) count++;
    }
    return count;
}

void expect(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[통과] " << name << "\n";
        passed++;
    } else {
        std::cerr << "[실패] " << name << "\n";
        failed++;
    }
}

} // namespace

int main() {
    {
        const auto violations = check(
            "module domain\n"
            "module service depends domain\n"
            "module ui depends service\n"
            "layer ui above service\n"
            "layer service above domain\n");
        expect(violations.empty(), "정상 구조 통과");
    }

    {
        const auto violations = check(
            "module ui depends missing\n");
        expect(countKind(violations, Violation::Kind::ImplicitDependency) == 1,
               "미선언 모듈 의존 검출");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends a\n");
        expect(countKind(violations, Violation::Kind::CyclicDependency) == 1,
               "직접 순환 의존 검출");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends c\n"
            "module c depends a\n");
        expect(countKind(violations, Violation::Kind::CyclicDependency) == 1,
               "다단계 순환 의존 검출");
    }

    {
        const auto violations = check(
            "module loop depends loop\n");
        expect(countKind(violations, Violation::Kind::CyclicDependency) == 1,
               "자기 자신을 향한 순환 의존 검출");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module data depends ui\n"
            "layer ui above data\n");
        expect(countKind(violations, Violation::Kind::LayerViolation) == 1,
               "하위 계층에서 상위 계층으로 향하는 의존 검출");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module service\n"
            "module data depends ui\n"
            "layer ui above service\n"
            "layer service above data\n");
        expect(countKind(violations, Violation::Kind::LayerViolation) == 1,
               "하위 계층에서 전이적 상위 계층으로 향하는 의존 검출");
    }

    {
        const auto violations = check(
            "module ui depends data, missing\n"
            "module data depends ui\n"
            "layer ui above data\n");
        expect(countKind(violations, Violation::Kind::ImplicitDependency) == 1 &&
               countKind(violations, Violation::Kind::CyclicDependency) == 1 &&
               countKind(violations, Violation::Kind::LayerViolation) == 1,
               "서로 다른 구조 위반 동시 검출");
    }

    {
        const auto violations = check(
            "# 의존이 없는 독립 모듈\n"
            "module alpha\n"
            "module beta\n");
        expect(violations.empty(), "독립 모듈 여러 개 통과");
    }

    {
        const auto violations = check(
            "module service\n"
            "module service\n");
        expect(countKind(violations, Violation::Kind::DuplicateModule) == 1,
               "중복 모듈 선언 검출");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer missing_upper above missing_lower\n");
        expect(countKind(violations, Violation::Kind::UndefinedLayerModule) == 2,
               "계층 선언의 미선언 모듈 참조 검출");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer service above service\n");
        expect(countKind(violations, Violation::Kind::SelfLayer) == 1,
               "자기 자신을 향한 계층 선언 검출");
    }

    std::cout << "\n검사기 테스트: " << passed << "개 통과, "
              << failed << "개 실패\n";
    return failed == 0 ? 0 : 1;
}
