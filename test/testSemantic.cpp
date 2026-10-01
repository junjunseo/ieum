#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "lexer.h"
#include "parser.h"
#include "semantic.h"

namespace {

int passed = 0;
int failed = 0;

struct Analysis {
    Program program;
    SemanticResult semantics;
};

Analysis analyze(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    Program program = parser.parse();
    SemanticAnalyzer analyzer(program);
    SemanticResult semantics = analyzer.analyze();
    return Analysis{std::move(program), std::move(semantics)};
}

int countKind(
    const std::vector<SemanticViolation>& violations,
    SemanticViolationKind kind) {
    int count = 0;
    for (const auto& violation : violations) {
        if (violation.kind == kind) count++;
    }
    return count;
}

bool hasViolationLine(
    const std::vector<SemanticViolation>& violations,
    SemanticViolationKind kind,
    int line) {
    for (const auto& violation : violations) {
        if (violation.kind == kind && violation.line == line) return true;
    }
    return false;
}

bool hasMessage(
    const std::vector<SemanticViolation>& violations,
    SemanticViolationKind kind,
    const std::string& text) {
    for (const auto& violation : violations) {
        if (violation.kind == kind &&
            violation.message.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string qualifiedName(const Program& program, const FunctionRef& ref) {
    return program.modules[ref.module].name + "." +
        program.modules[ref.module].functions[ref.function].name;
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
        std::cerr << "[FAIL] expected " << expected
                  << " semantic assertions, got " << passed << "\n";
        failed++;
        assert(passed == expected);
    }
}

} // namespace

int main() {
    {
        const Analysis analysis = analyze(
            "module data {\n"
            "  fn save(item) {}\n"
            "}\n"
            "module service depends data {\n"
            "  let shared\n"
            "  fn prepare(item) {}\n"
            "  fn main() {\n"
            "    let local\n"
            "    call prepare(local)\n"
            "    call save(shared)\n"
            "  }\n"
            "}\n");
        assertTrue(analysis.semantics.ok(),
                   "accepts local and dependency function calls");
        assertTrue(analysis.semantics.resolvedCalls.size() == 2,
                   "resolves every valid call");
        assertTrue(
            qualifiedName(
                analysis.program,
                analysis.semantics.resolvedCalls[0].target) == "service.prepare",
            "prefers current module function");
        assertTrue(
            qualifiedName(
                analysis.program,
                analysis.semantics.resolvedCalls[1].target) == "data.save",
            "resolves function from declared dependency");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  let state\n"
            "  let state\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::DuplicateModuleVariable) == 1,
                   "detects duplicate module variable");
        assertTrue(hasViolationLine(
                       violations,
                       SemanticViolationKind::DuplicateModuleVariable,
                       3),
                   "reports duplicate module variable line");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn run() {}\n"
            "  fn run() {}\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::DuplicateFunction) == 1,
                   "detects duplicate function");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn run(value, value) {}\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::DuplicateParameter) == 1,
                   "detects duplicate parameter");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn run(value) {\n"
            "    let value\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::DuplicateLocalVariable) == 1,
                   "detects local variable colliding with parameter");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn run() {\n"
            "    let value\n"
            "    let value\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::DuplicateLocalVariable) == 1,
                   "detects duplicate local variable");
        assertTrue(hasViolationLine(
                       violations,
                       SemanticViolationKind::DuplicateLocalVariable,
                       4),
                   "reports duplicate local variable line");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn helper(value) {}\n"
            "  fn run() {\n"
            "    call helper(value)\n"
            "    let value\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::UndefinedVariable) == 1,
                   "rejects local variable used before declaration");
        assertTrue(hasViolationLine(
                       violations,
                       SemanticViolationKind::UndefinedVariable,
                       4),
                   "reports undefined argument line");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn run() {\n"
            "    call missing()\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::UndefinedFunction) == 1,
                   "detects undefined function");
    }

    {
        const auto violations = analyze(
            "module app {\n"
            "  fn helper(value) {}\n"
            "  fn run() {\n"
            "    call helper()\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::ArityMismatch) == 1,
                   "detects function arity mismatch");
    }

    {
        const auto violations = analyze(
            "module data {\n"
            "  fn save(item) {}\n"
            "}\n"
            "module service {\n"
            "  let item\n"
            "  fn run() {\n"
            "    call save(item)\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::MissingCallDependency) == 1,
                   "detects cross-module call without depends");
        assertTrue(hasMessage(
                       violations,
                       SemanticViolationKind::MissingCallDependency,
                       "data"),
                   "reports required dependency module");
    }

    {
        const auto violations = analyze(
            "module left {\n"
            "  fn load() {}\n"
            "}\n"
            "module right {\n"
            "  fn load() {}\n"
            "}\n"
            "module app depends left, right {\n"
            "  fn run() {\n"
            "    call load()\n"
            "  }\n"
            "}\n").semantics.violations;
        assertTrue(countKind(
                       violations,
                       SemanticViolationKind::AmbiguousFunction) == 1,
                   "detects ambiguous dependency function");
    }

    {
        const Analysis analysis = analyze(
            "module dependency {\n"
            "  fn run() {}\n"
            "}\n"
            "module app depends dependency {\n"
            "  fn run() {}\n"
            "  fn main() {\n"
            "    call run()\n"
            "  }\n"
            "}\n");
        assertTrue(analysis.semantics.ok(),
                   "allows local function to shadow dependency function");
        assertTrue(
            qualifiedName(
                analysis.program,
                analysis.semantics.resolvedCalls[0].target) == "app.run",
            "resolves shadowed call to local function");
    }

    {
        const Analysis analysis = analyze(
            "module app {\n"
            "  let value\n"
            "  fn helper(item) {}\n"
            "  fn run() {\n"
            "    let value\n"
            "    call helper(value)\n"
            "  }\n"
            "}\n");
        assertTrue(analysis.semantics.ok(),
                   "allows local variable to shadow module variable");
    }

    {
        const auto semantics = analyze(
            "module app {\n fn run() {\n call run()\n }\n}\n").semantics;
        assertTrue(semantics.ok(), "allows direct recursion under runtime limits");
        assertTrue(semantics.resolvedCalls.size() == 1 && semantics.resolvedCalls[0].line == 3,
                   "resolves recursive call and source line");
    }

    {
        const auto semantics = analyze(
            "module app {\n fn first() {\n call second()\n }\n"
            " fn second() {\n call first()\n }\n}\n").semantics;
        assertTrue(semantics.ok(), "allows mutual function recursion");
        assertTrue(semantics.resolvedCalls.size() == 2, "resolves both recursive edges");
    }

    {
        const Analysis analysis = analyze(
            "module data\n"
            "module service depends data\n");
        assertTrue(analysis.semantics.ok(),
                   "accepts legacy modules without bodies");
    }

    assertTestCount(26);

    std::cout << "\nSemantic tests: " << passed << " passed, "
              << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
