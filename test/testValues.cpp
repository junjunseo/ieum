#include <iostream>
#include <limits>
#include <set>
#include <functional>
#include "checker.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"

namespace {
int passed = 0, failed = 0;
void check(bool ok, const std::string& name) {
    if (ok) { ++passed; std::cout << "[PASS] " << name << "\n"; }
    else { ++failed; std::cerr << "[FAIL] " << name << "\n"; }
}
Program parse(const std::string& source) {
    return Parser(Lexer(source, "values.ieum").tokenize()).parse();
}
std::string wrap(const std::string& body) {
    return "module app {\n  fn main() {\n" + body + "\n  }\n}\n";
}
ExecutionResult run(const std::string& source) {
    auto program = parse(source);
    if (!Checker(program).check().empty()) throw std::runtime_error("structural error");
    auto semantics = SemanticAnalyzer(program).analyze();
    if (!semantics.ok()) throw std::runtime_error(semantics.violations.front().message);
    return Interpreter(program, semantics).run("app", "main");
}
void expectValue(const std::string& expression, Value expected) {
    try {
        const auto result = run(wrap("    let result = " + expression));
        check(result.success && result.entryLocals.at("result") == expected, expression);
    } catch (const std::exception& e) { check(false, expression + ": " + e.what()); }
}
void staticError(const std::string& source, SemanticViolationKind kind, const std::string& name) {
    try {
        auto program = parse(source);
        auto semantics = SemanticAnalyzer(program).analyze();
        bool found = false;
        for (const auto& error : semantics.violations) if (error.kind == kind) found = true;
        check(found && !Interpreter(program, semantics).run("app", "main").success, name);
    } catch (const std::exception& e) { check(false, name + ": " + e.what()); }
}
void runtimeError(const std::string& expression, const std::string& code) {
    try {
        const auto result = run(wrap("    let result = " + expression));
        check(!result.success && result.error.find(code) != std::string::npos &&
            result.error.find("values.ieum:3:") != std::string::npos, expression + " -> " + code);
    } catch (const std::exception& e) { check(false, expression + ": " + e.what()); }
}
void syntaxError(const std::string& expression, const std::string& name) {
    try { parse(wrap("    let result = " + expression)); check(false, name); }
    catch (const std::runtime_error& e) { check(std::string(e.what()).find("values.ieum:3:") != std::string::npos, name); }
}
}

int main() {
    const auto lo = std::numeric_limits<std::int64_t>::min();
    const auto hi = std::numeric_limits<std::int64_t>::max();
    const std::vector<std::pair<std::string, std::int64_t>> integers = {
        {"1 + 2 * 3", 7}, {"(1 + 2) * 3", 9}, {"20 - 5 - 3", 12},
        {"24 / 4 / 2", 3}, {"17 % 5 + 2", 4}, {"-7 / 3", -2},
        {"-7 % 3", -1}, {"7 % -3", 1}, {"+5 * -2", -10},
        {"-(2 + 3)", -5}, {"--2", 2}, {"00042", 42},
        {"9223372036854775807", hi}, {"-9223372036854775808", lo},
        {"-9223372036854775808 + 1", lo + 1}, {"9223372036854775807 - 1", hi - 1},
        {"-9223372036854775808 * 0", 0}, {"0 * -9223372036854775808", 0},
        {"-9223372036854775808 * 1", lo}, {"1 * -9223372036854775808", lo},
        {"-2 * -3", 6}, {"-9223372036854775808 - -9223372036854775808", 0},
        {"-9223372036854775808 + 9223372036854775807", -1}
    };
    for (const auto& [expr, expected] : integers) expectValue(expr, Value{expected});
    for (const auto& expr : {"true", "!false", "1 < 2", "2 <= 2", "3 > 2", "3 >= 3",
                            "1 + 2 == 3", "2 != 3", "true || false && false", "() == ()",
                            "\"가\" == \"가\"", "true || (1 / 0 == 1)"}) expectValue(expr, Value{true});
    for (const auto& expr : {"false", "!true", "1 > 2", "true == false", "() != ()",
                            "false && (1 / 0 == 1)", "false && (9223372036854775807 + 1 == 0)"}) expectValue(expr, Value{false});
    expectValue("\"안녕\" + \" 이음 # 😀\"", Value{std::string("안녕 이음 # 😀")});
    expectValue("\"a\\n\\r\\t\\\\\\\"z\"", Value{std::string("a\n\r\t\\\"z")});
    expectValue("\"\"", Value{std::string{}});
    expectValue("()", Value{std::monostate{}});

    for (const auto& expr : {"9223372036854775807 + 1", "-9223372036854775808 - 1",
         "9223372036854775807 - -1", "-9223372036854775808 + -1", "9223372036854775807 * 2",
         "-9223372036854775808 * -1", "-1 * -9223372036854775808", "-9223372036854775808 * 2",
         "-(-9223372036854775808)", "-9223372036854775808 / -1", "-9223372036854775808 % -1"}) runtimeError(expr, "integer_overflow");
    runtimeError("1 / 0", "division_by_zero");
    runtimeError("1 % 0", "division_by_zero");
    runtimeError("true && (1 / 0 == 0)", "division_by_zero");
    runtimeError("false || (1 / 0 == 0)", "division_by_zero");

    for (const auto& expr : {"1 + true", "\"1\" + 2", "1 && 2", "!1", "-true", "true < false",
                            "1 == true", "() + ()", "false && 1", "\"a\" - \"b\""}) {
        staticError(wrap("    let result = " + std::string(expr)), SemanticViolationKind::TypeMismatch, expr);
    }
    staticError(wrap("    let n: int = true"), SemanticViolationKind::TypeMismatch, "annotation mismatch");
    staticError(wrap("    let n: int"), SemanticViolationKind::TypeMismatch, "typed value requires initialization");
    staticError(wrap("    let n = 1\n    n = false"), SemanticViolationKind::TypeMismatch, "reassignment type mismatch");
    staticError(wrap("    let n = n + 1"), SemanticViolationKind::UndefinedVariable, "self reference before declaration");
    staticError(wrap("    missing = 1"), SemanticViolationKind::UndefinedVariable, "assignment to missing variable");
    staticError(wrap("    let n = future\n    let future = 1"), SemanticViolationKind::UndefinedVariable, "forward local reference");
    staticError(wrap("    let n = false && missing"), SemanticViolationKind::UndefinedVariable, "short circuit still checks names");
    staticError("module app {\n let a = b\n let b = 1\n fn main() {}\n}\n", SemanticViolationKind::UndefinedVariable, "forward module initializer");
    staticError("module app {\n fn f(n: int) {}\n fn main() {\n let b = true\n call f(b)\n }\n}\n", SemanticViolationKind::TypeMismatch, "typed argument mismatch");
    staticError("module app {\n fn f(n) {}\n fn main() {\n let b = 1\n call f(b)\n }\n}\n", SemanticViolationKind::TypeMismatch, "legacy parameter remains unit");
    staticError(wrap("    let n = 1\n    let n = 2"), SemanticViolationKind::DuplicateLocalVariable, "duplicate initialized local");
    staticError("module app {\n let n = 1\n fn main() {\n n = true\n }\n}\n", SemanticViolationKind::TypeMismatch, "module reassignment type mismatch");

    for (const auto& expr : {"9223372036854775808", "-9223372036854775809", "18446744073709551616",
                            "1 +", "(1 + 2", "1 2", "1.5", "\"bad\\q\"", "\"unfinished", "\"line\nend\""}) syntaxError(expr, expr);
    syntaxError(std::string("\"") + char(0xC0) + char(0xAF) + "\"", "reject overlong UTF-8");
    syntaxError(std::string("\"") + char(0xED) + char(0xA0) + char(0x80) + "\"", "reject UTF-8 surrogate");
    syntaxError(std::string(200, '(') + "1" + std::string(200, ')'), "bound nested expression");
    syntaxError(std::string(200, '!') + "true", "bound unary expression");
    std::string chain = "1";
    for (int i = 0; i < 200; ++i) chain += "+1";
    syntaxError(chain, "bound left-deep AST");

    try {
        const auto result = run("module app {\n let total: int = 1\n let copy = total + 2\n fn set(n: int) {\n total = total + n\n n = 99\n }\n fn main() {\n let n = 6\n call set(n)\n call set(n)\n let total = total + 10\n total = total + 1\n let unit\n let explicit: unit = ()\n }\n}\n");
        check(result.success && std::get<std::int64_t>(result.moduleValues.at("app.total")) == 13, "module values persist across calls");
        check(std::get<std::int64_t>(result.moduleValues.at("app.copy")) == 3, "module initialization in declaration order");
        check(std::get<std::int64_t>(result.entryLocals.at("n")) == 6, "parameter assignment does not change caller");
        check(std::get<std::int64_t>(result.entryLocals.at("total")) == 24, "initializer sees outer value then local shadows it");
        check(valueType(result.entryLocals.at("unit")) == ValueType::Unit, "legacy variable named unit remains valid");
        check(result.callsExecuted == 2, "existing call trace counts preserved");
    } catch (const std::exception& e) { check(false, e.what()); }
    try {
        auto p = parse("module data {\n let stored = 0\n fn save(n: int) {\n stored = stored + n\n }\n}\nmodule app depends data {\n fn main() {\n let v = 7\n call save(v)\n }\n}\n");
        auto s = SemanticAnalyzer(p).analyze();
        auto interpreter = Interpreter(p, s);
        auto a = interpreter.run("app", "main");
        auto b = interpreter.run("app", "main");
        check(a.success && std::get<std::int64_t>(a.moduleValues.at("data.stored")) == 7, "typed value crosses declared dependency");
        check(a.moduleValues == b.moduleValues && a.events.size() == b.events.size(), "each run starts with fresh globals");
        check(s.findCallByNode(p.modules[1].functions[0].body[1].id) != nullptr, "call resolution uses stable statement ID");
    } catch (const std::exception& e) { check(false, e.what()); }
    try {
        const auto result = run("module app {\n let bad = 1 / 0\n fn main() {}\n}\n");
        check(!result.success && result.functionsExecuted == 0 && result.error.find("values.ieum:2:") != std::string::npos,
              "module initializer failure prevents entry execution");
        const auto copied = run("module app {\n let text = \"\"\n let flag = false\n fn copy(s: string, b: bool) {\n text = s + \"!\"\n flag = b\n s = \"changed\"\n }\n fn main() {\n let s = \"hello\"\n let b = true\n call copy(s, b)\n }\n}\n");
        check(copied.success && std::get<std::string>(copied.moduleValues.at("app.text")) == "hello!" &&
              std::get<bool>(copied.moduleValues.at("app.flag")) && std::get<std::string>(copied.entryLocals.at("s")) == "hello",
              "string and bool arguments are copied into typed parameters");
    } catch (const std::exception& e) { check(false, e.what()); }
    try {
        auto tokens = Lexer("\xEF\xBB\xBFmodule app {\r\n  let n = 7\r\n}\r\n", "positions.ieum").tokenize();
        check(tokens[0].span.column == 1 && tokens[4].span.line == 2 && tokens[4].span.column == 3,
              "BOM and CRLF source positions");
        auto p = parse(wrap("    let n = (1 + 2) * -3\n    n = n + 1"));
        std::set<NodeId> ids;
        bool unique = true;
        std::function<void(const Expr&)> visit = [&](const Expr& expr) {
            if (!expr) return;
            unique = unique && expr->id != 0 && ids.insert(expr->id).second;
            visit(expr->left); visit(expr->right);
        };
        for (const auto& statement : p.modules[0].functions[0].body) {
            unique = unique && ids.insert(statement.id).second;
            visit(statement.expression);
        }
        check(unique, "expression and statement IDs unique within program");
        const auto expr = p.modules[0].functions[0].body[0].expression;
        check(expr->span.file == "values.ieum" && expr->span.line == 3 && expr->span.column == 13 && expr->span.endColumn == 25,
              "full expression source span");
        const auto s = SemanticAnalyzer(parse(wrap("    let n = 1 + true"))).analyze();
        check(s.violations[0].span.file == "values.ieum" && s.violations[0].span.line == 3,
              "type errors preserve source span");
    } catch (const std::exception& e) { check(false, e.what()); }
    std::cout << "Values tests: " << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
