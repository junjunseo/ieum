#include <cassert>
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

bool hasViolationLine(const std::vector<Violation>& violations,
                      Violation::Kind kind,
                      int line) {
    for (const auto& violation : violations) {
        if (violation.kind == kind && violation.line == line) return true;
    }
    return false;
}

void assertTrue(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
        passed++;
        return;
    }

    std::cerr << "[FAIL] " << name << "\n";
    failed++;
    assert(condition);
}

void assertTestCount(int expected) {
    if (passed != expected) {
        std::cerr << "[FAIL] expected " << expected << " checker assertions, got "
                  << passed << "\n";
        failed++;
        assert(passed == expected);
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
        assertTrue(violations.empty(), "accepts valid layered structure");
    }

    {
        const auto violations = check(
            "# independent modules\n"
            "module alpha\n"
            "module beta\n");
        assertTrue(violations.empty(), "accepts independent modules");
    }

    {
        const auto violations = check(
            "module ui depends service\n"
            "module service\n"
            "layer ui above service\n");
        assertTrue(violations.empty(), "allows upper layer to depend on lower layer");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module service depends repository\n"
            "module repository\n"
            "layer ui above service\n");
        assertTrue(violations.empty(), "allows dependency outside declared layer relation");
    }

    {
        const auto violations = check("module ui depends missing\n");
        assertTrue(countKind(violations, Violation::Kind::ImplicitDependency) == 1,
                   "detects one undefined dependency target");
    }

    {
        const auto violations = check("module ui depends missing_api, missing_db\n");
        assertTrue(countKind(violations, Violation::Kind::ImplicitDependency) == 2,
                   "detects multiple undefined dependency targets");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends a\n");
        assertTrue(countKind(violations, Violation::Kind::CyclicDependency) == 1,
                   "detects direct dependency cycle");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends a\n");
        assertTrue(hasViolationLine(violations, Violation::Kind::CyclicDependency, 2),
                   "reports cycle at closing dependency line");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends c\n"
            "module c depends a\n");
        assertTrue(countKind(violations, Violation::Kind::CyclicDependency) == 1,
                   "detects multi-step dependency cycle");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends c\n"
            "module c depends a\n");
        assertTrue(hasViolationLine(violations, Violation::Kind::CyclicDependency, 3),
                   "reports multi-step cycle at closing dependency line");
    }

    {
        const auto violations = check("module loop depends loop\n");
        assertTrue(countKind(violations, Violation::Kind::CyclicDependency) == 1,
                   "detects self dependency cycle");
    }

    {
        const auto violations = check(
            "module a depends b\n"
            "module b depends a\n"
            "module c depends d\n"
            "module d depends c\n");
        assertTrue(countKind(violations, Violation::Kind::CyclicDependency) == 2,
                   "detects multiple independent dependency cycles");
    }

    {
        const auto violations = check(
            "module a depends b, c\n"
            "module b depends a\n"
            "module c depends a\n");
        assertTrue(countKind(violations, Violation::Kind::CyclicDependency) == 2,
                   "detects overlapping cycles from one module");
    }

    {
        const auto violations = check("module a depends missing\n");
        assertTrue(countKind(violations, Violation::Kind::CyclicDependency) == 0,
                   "does not report cycles for missing dependency targets");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module data depends ui\n"
            "layer ui above data\n");
        assertTrue(countKind(violations, Violation::Kind::LayerViolation) == 1,
                   "detects direct upward layer dependency");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module service\n"
            "module data depends ui\n"
            "layer ui above service\n"
            "layer service above data\n");
        assertTrue(countKind(violations, Violation::Kind::LayerViolation) == 1,
                   "detects transitive upward layer dependency");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module helper depends ui\n"
            "module data depends helper\n"
            "layer ui above data\n");
        assertTrue(countKind(violations, Violation::Kind::LayerViolation) == 1,
                   "detects indirect upward dependency path");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module helper depends ui\n"
            "module data depends helper\n"
            "layer ui above data\n");
        assertTrue(hasViolationLine(violations, Violation::Kind::LayerViolation, 3),
                   "reports indirect layer violation at lower module line");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module service depends ui\n"
            "module data depends service\n"
            "layer ui above service\n"
            "layer service above data\n");
        assertTrue(countKind(violations, Violation::Kind::LayerViolation) == 3,
                   "detects every upward dependency path through layer chain");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module service depends ui\n"
            "module data depends service, ui\n"
            "layer ui above service\n"
            "layer service above data\n");
        assertTrue(countKind(violations, Violation::Kind::LayerViolation) == 3,
                   "detects multiple layer violations in one program");
    }

    {
        const auto violations = check(
            "module service\n"
            "module service\n");
        assertTrue(countKind(violations, Violation::Kind::DuplicateModule) == 1,
                   "detects duplicate module declaration");
    }

    {
        const auto violations = check(
            "module service\n"
            "module service\n"
            "module service\n");
        assertTrue(countKind(violations, Violation::Kind::DuplicateModule) == 2,
                   "detects repeated duplicate module declarations");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer missing above service\n");
        assertTrue(countKind(violations, Violation::Kind::UndefinedLayerModule) == 1,
                   "detects undefined upper layer module");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer service above missing\n");
        assertTrue(countKind(violations, Violation::Kind::UndefinedLayerModule) == 1,
                   "detects undefined lower layer module");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer missing_upper above missing_lower\n");
        assertTrue(countKind(violations, Violation::Kind::UndefinedLayerModule) == 2,
                   "detects both undefined layer modules");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer service above service\n");
        assertTrue(countKind(violations, Violation::Kind::SelfLayer) == 1,
                   "detects self layer declaration");
    }

    {
        const auto violations = check(
            "module service\n"
            "layer service above service\n");
        assertTrue(countKind(violations, Violation::Kind::LayerViolation) == 0,
                   "does not turn self layer declaration into layer violation");
    }

    {
        const auto violations = check(
            "module ui depends data, missing\n"
            "module data depends ui\n"
            "layer ui above data\n");
        assertTrue(countKind(violations, Violation::Kind::ImplicitDependency) == 1 &&
                   countKind(violations, Violation::Kind::CyclicDependency) == 1 &&
                   countKind(violations, Violation::Kind::LayerViolation) == 1,
                   "detects mixed structural violations together");
    }

    {
        const auto violations = check(
            "module service\n"
            "module service\n");
        assertTrue(hasViolationLine(violations, Violation::Kind::DuplicateModule, 2),
                   "reports duplicate module line");
    }

    {
        const auto violations = check(
            "module ui\n"
            "module data depends ui\n"
            "layer ui above data\n");
        assertTrue(hasViolationLine(violations, Violation::Kind::LayerViolation, 2),
                   "reports layer violation at depending module line");
    }

    {
        const auto violations = check(
            "module a\n"
            "module a depends a\n");
        assertTrue(countKind(violations, Violation::Kind::DuplicateModule) == 1 &&
                   countKind(violations, Violation::Kind::CyclicDependency) == 0,
                   "keeps first module declaration as graph source after duplicate");
    }

    assertTestCount(31);

    std::cout << "\nChecker tests: " << passed << " passed, "
              << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
