#include <functional>
#include <iostream>
#include <set>
#include <stdexcept>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "interpreter.h"
#include "checker.h"

static int passed = 0, failed = 0;
static void check(bool condition, const std::string& label) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << label << "\n";
    condition ? ++passed : ++failed;
}
static Program parse(const std::string& text) {
    return Parser(Lexer(text, "control.ieum").tokenize()).parse();
}
static std::string app(const std::string& body, const std::string& functions = "", const std::string& type = "int") {
    return "module app {\n" + functions + "fn main() -> " + type + " {\n" + body + "\n}\n}\n";
}
static ExecutionResult run(const std::string& text, ExecutionLimits limits = {}) {
    const auto program = parse(text);
    if (!Checker(program).check().empty()) throw std::runtime_error("unexpected structural error");
    const auto semantics = SemanticAnalyzer(program).analyze();
    if (!semantics.ok()) throw std::runtime_error(semantics.violations.front().message);
    return Interpreter(program, semantics, limits).run("app", "main");
}
static void expect(const std::string& label, const std::string& text, Value value, ExecutionLimits limits = {}) {
    try {
        const auto result = run(text, limits);
        check(result.success && result.returnValue == value, label + (result.success ? "" : ": " + result.error));
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
static void rejects(const std::string& label, const std::string& text, SemanticViolationKind kind) {
    try {
        const auto program = parse(text);
        const auto semantics = SemanticAnalyzer(program).analyze();
        bool found = false;
        for (const auto& violation : semantics.violations) found |= violation.kind == kind;
        check(found && !Interpreter(program, semantics).run("app", "main").success, label);
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
static void runtimeError(const std::string& label, const std::string& text, const std::string& code, ExecutionLimits limits = {}) {
    try {
        const auto result = run(text, limits);
        check(!result.success && result.error.find(code) != std::string::npos &&
            result.error.find("control.ieum:") != std::string::npos, label);
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
static void parseError(const std::string& label, const std::string& text) {
    try { parse(text); check(false, label); }
    catch (const std::exception& e) { check(std::string(e.what()).find("control.ieum:") != std::string::npos, label); }
}

int main() {
    const std::string factorial = "fn fact(n: int) -> int {\n if n <= 1 {\n return 1\n }\n return n * fact(n - 1)\n}\n";
    const std::string identity = "fn id(n: int) -> int {\n return n\n}\n";
    expect("sum 1 through 10 is 55", app("let sum = 0\nlet i = 1\nwhile i <= 10 {\nsum = sum + i\ni = i + 1\n}\nreturn sum"), std::int64_t{55});
    expect("factorial(5) is 120", app("return fact(5)", factorial), std::int64_t{120});
    expect("nested calls and expression arguments", app("return id(id(2) + 3) * id(4)", identity), std::int64_t{20});
    expect("recursive calls preserve caller operands", app("return fact(4) + fact(3) * fact(2)", factorial), std::int64_t{36});
    expect("calls compose with unary operators", app("return -id(3) + +id(8)", identity), std::int64_t{5});
    expect("string return", app("return greet(\"이음\")", "fn greet(s: string) -> string {\nreturn \"안녕 \" + s\n}\n", "string"), std::string("안녕 이음"));
    expect("bool return and typed unit parameter", app("return yes(())", "fn yes(u: unit) -> bool {\nreturn u == ()\n}\n", "bool"), true);
    expect("bare unit return skips later statements", app("return\nlet bad = 1 / 0", "", "unit"), std::monostate{});
    expect("explicit unit return", app("return ()", "", "unit"), std::monostate{});
    expect("unit fallthrough and legacy untyped parameters", app("call old(())\nreturn", "fn old(x) {}\n", "unit"), std::monostate{});
    expect("call statement discards non-unit return", app("call id(3 + 4)\nreturn 8", identity), std::int64_t{8});
    expect("unit call expression", app("let u = old(())\nreturn u == ()", "fn old(x) {}\n", "bool"), true);

    expect("both if paths return", app("if true {\nreturn 1\n} else {\nreturn 2\n}"), std::int64_t{1});
    expect("else runs and accepts next-line keyword", app("if false {\nreturn 1\n}\nelse {\nreturn 2\n}"), std::int64_t{2});
    expect("else if chain", app("if false {\nreturn 1\n} else if true {\nreturn 2\n} else {\nreturn 3\n}"), std::int64_t{2});
    expect("empty blocks and skipped loop", app("{}\nif true {} else {}\nwhile false {}\nreturn 7"), std::int64_t{7});
    expect("block return", app("{\nreturn 9\n}\nreturn 0"), std::int64_t{9});
    expect("nested return escapes loops", app("while true {\n{\nif true {\nreturn 42\n}\n}\n}\nreturn 0"), std::int64_t{42});
    expect("shadow initialized from outer variable", app("let x = 3\n{\nlet x = x + 1\nx = 99\n}\nreturn x"), std::int64_t{3});
    expect("assignment finds nearest visible scope", app("let x = 3\n{\n{\nx = x + 4\n}\n}\nreturn x"), std::int64_t{7});
    expect("sibling blocks can reuse names", app("let total = 0\n{\nlet x = 2\ntotal = x\n}\n{\nlet x = 3\ntotal = total + x\n}\nreturn total"), std::int64_t{5});
    expect("parameter can be shadowed in nested block", app("return f(3)", "fn f(x: int) -> int {\n{\nlet x = 8\n}\nreturn x\n}\n"), std::int64_t{3});
    expect("fresh loop scope and continue unwind", app("let i = 0\nlet sum = 0\nwhile i < 6 {\nlet old = i\ni = i + 1\n{\nif i % 2 == 0 {\ncontinue\n}\n}\nsum = sum + old + 1\n}\nreturn sum"), std::int64_t{9});
    expect("break exits nearest nested loop", app("let i = 0\nlet n = 0\nwhile i < 3 {\ni = i + 1\nwhile true {\nn = n + 1\n{\nbreak\n}\n}\nn = n + 10\n}\nreturn n"), std::int64_t{33});
    expect("break unwinds scopes without leaking shadow", app("let x = 1\nwhile true {\nlet x = 3\n{\nbreak\n}\n}\nreturn x"), std::int64_t{1});
    expect("callee return preserves caller loop", app("let sum = 0\nlet i = 0\nwhile i < 3 {\ni = i + 1\nsum = sum + id(i)\n}\nreturn sum", identity), std::int64_t{6});
    expect("call frame isolates locals", app("let x = 5\ncall mutate(x)\nreturn x", "fn mutate(x: int) {\nx = 100\nlet other = 7\n}\n"), std::int64_t{5});
    expect("callee sees module rather than caller local", app("let x = 5\nreturn read()", "let x = 2\nfn read() -> int {\nreturn x\n}\n"), std::int64_t{2});

    const std::string bump = "let n = 0\nfn bump() -> int {\nn = n + 1\nreturn n\n}\nfn positive() -> bool {\nreturn bump() > 0\n}\n";
    expect("short circuit skips side-effecting calls", app("let a = true || positive()\nlet b = false && positive()\nreturn n", bump), std::int64_t{0});
    expect("logical RHS calls execute when required", app("let a = false || positive()\nlet b = true && positive()\nreturn n", bump), std::int64_t{2});
    expect("binary calls evaluated left to right", app("return bump() * 10 + bump()", bump), std::int64_t{12});
    expect("arguments evaluated left to right with nested frames", app("return combine(bump(), bump())", bump + "fn combine(a: int, b: int) -> int {\nreturn a * 10 + b\n}\n"), std::int64_t{12});
    expect("loop condition calls reevaluate", app("let sum = 0\nwhile bump() <= 3 {\nsum = sum + n\n}\nreturn sum * 10 + n", bump), std::int64_t{64});
    expect("module initializer can call typed function", app("return value", "let value = id(7) + id(8)\n" + identity), std::int64_t{15});
    runtimeError("indirect forward global read diagnosed", app("return value", "let value = read()\nlet later = 3\nfn read() -> int {\nreturn later\n}\n"), "uninitialized_variable");
    expect("mutual recursion terminates", app("return even(12)", "fn even(n: int) -> bool {\nif n == 0 {\nreturn true\n}\nreturn odd(n - 1)\n}\nfn odd(n: int) -> bool {\nif n == 0 {\nreturn false\n}\nreturn even(n - 1)\n}\n", "bool"), true);
    const std::string countdown = "fn down(n: int) -> int {\nif n == 0 {\nreturn 0\n}\nreturn down(n - 1)\n}\n";
    expect("1500 recursive frames use heap continuations", app("return down(1500)", countdown), std::int64_t{0}, {100000, 2000});
    runtimeError("default depth bounds deep recursion", app("return down(1500)", countdown), "call_depth_limit");
    runtimeError("direct infinite recursion bounded", app("return forever()", "fn forever() -> int {\nreturn forever()\n}\n"), "call_depth_limit", {10000, 16});
    runtimeError("infinite unit recursion bounded", app("call main()", "", "unit"), "call_depth_limit", {10000, 8});
    runtimeError("empty infinite loop bounded", app("while true {}", "", "unit"), "step_limit", {75, 8});
    runtimeError("continue loop bounded", app("while true {\ncontinue\n}", "", "unit"), "step_limit", {75, 8});
    runtimeError("step budget covers recursion", app("return down(100)", countdown), "step_limit", {75, 1000});
    runtimeError("depth counts entry frame", app("return id(1)", identity), "call_depth_limit", {1000, 1});
    expect("single entry frame fits depth one", app("return 1"), std::int64_t{1}, {1000, 1});
    runtimeError("overflow in returned expression", app("return id(9223372036854775807) + 1", identity), "integer_overflow");
    runtimeError("division failure inside called function", app("return bad()", "fn bad() -> int {\nreturn 1 / 0\n}\n"), "division_by_zero");
    try {
        const auto result = run(app("while true {}", "", "unit"), {31, 8});
        check(!result.success && result.stepsExecuted == 31, "execution never exceeds step budget");
        const auto success = run(app("return 2"));
        check(run(app("return 2"), {success.stepsExecuted, 1}).success, "exact required step budget succeeds");
        check(!run(app("return 2"), {success.stepsExecuted - 1, 1}).success, "one fewer step fails");
        check(!run(app("return 2"), {0, 1}).success && !run(app("return 2"), {100, 0}).success, "API rejects zero limits");
    } catch (const std::exception& e) { check(false, e.what()); }

    using K = SemanticViolationKind;
    rejects("non-unit function missing return", app("let x = 1"), K::MissingReturn);
    rejects("if without else may fall through", app("if true {\nreturn 1\n}"), K::MissingReturn);
    rejects("loop return conservatively requires trailing return", app("while true {\nreturn 1\n}"), K::MissingReturn);
    rejects("return value has wrong type", app("return false"), K::TypeMismatch);
    rejects("bare return needs unit function", app("return"), K::TypeMismatch);
    rejects("omitted return annotation remains unit", "module app {\nfn main() {\nreturn 1\n}\n}\n", K::TypeMismatch);
    rejects("non-unit parameters must have explicit types", app("return f(())", "fn f(x) -> int {\nreturn 1\n}\n"), K::IncompleteSignature);
    rejects("if condition must be bool", app("if 1 {}\nreturn 0"), K::TypeMismatch);
    rejects("while condition must be bool", app("while \"x\" {}\nreturn 0"), K::TypeMismatch);
    rejects("expression arity checked", app("return id()", identity), K::ArityMismatch);
    rejects("expression argument types checked", app("return id(true)", identity), K::TypeMismatch);
    rejects("call statement literal types checked", app("call id(\"1\")\nreturn 0", identity), K::TypeMismatch);
    rejects("unknown call expression", app("return missing()"), K::UndefinedFunction);
    rejects("short circuit still resolves calls", app("return true || missing()", "", "bool"), K::UndefinedFunction);
    rejects("unreachable code still type checked", app("return 1\nlet bad = 1 + true"), K::TypeMismatch);
    rejects("break outside loop", app("break\nreturn 0"), K::InvalidControlFlow);
    rejects("continue outside loop", app("{\ncontinue\n}\nreturn 0"), K::InvalidControlFlow);
    rejects("callee cannot break caller loop", app("while true {\ncall bad()\nbreak\n}\nreturn 0", "fn bad() {\nbreak\n}\n"), K::InvalidControlFlow);
    rejects("block local cannot escape", app("{\nlet x = 1\n}\nreturn x"), K::UndefinedVariable);
    rejects("branch local cannot escape", app("if true {\nlet x = 1\n}\nreturn x"), K::UndefinedVariable);
    rejects("loop local cannot escape", app("while false {\nlet x = 1\n}\nreturn x"), K::UndefinedVariable);
    rejects("sibling cannot see local", app("{\nlet x = 1\n}\n{\nreturn x\n}\nreturn 0"), K::UndefinedVariable);
    rejects("duplicate local in same nested scope", app("{\nlet x = 1\nlet x = 2\n}\nreturn 0"), K::DuplicateLocalVariable);
    rejects("root local conflicts with parameter", app("return f(2)", "fn f(x: int) -> int {\nlet x = 3\nreturn x\n}\n"), K::DuplicateLocalVariable);
    rejects("nested assignment preserves type", app("let x = 1\n{\nx = true\n}\nreturn x"), K::TypeMismatch);
    rejects("recursive call types checked", app("return f(1)", "fn f(x: int) -> int {\nreturn f(false)\n}\n"), K::TypeMismatch);
    rejects("global initializer call type checked", app("return 0", "let n: string = id(1)\n" + identity), K::TypeMismatch);

    const std::string data = "module data {\nfn answer() -> int {\nreturn 42\n}\n}\n";
    expect("expression call crosses declared dependency", data + "module app depends data {\nfn main() -> int {\nreturn answer()\n}\n}\nlayer app above data\n", std::int64_t{42});
    rejects("expression call needs depends", data + app("return answer()"), K::MissingCallDependency);
    rejects("unreachable call still needs depends", data + app("return 1\ncall answer()"), K::MissingCallDependency);
    rejects("global initializer call needs depends", data + app("return value", "let value = answer()\n"), K::MissingCallDependency);
    rejects("ambiguous expression call rejected", data + "module other {\nfn answer() -> int {\nreturn 2\n}\n}\nmodule app depends data, other {\nfn main() -> int {\nreturn answer()\n}\n}\n", K::AmbiguousFunction);
    try {
        check(!Checker(parse("module a depends b\nmodule b depends a\n")).check().empty(), "module dependency cycles remain forbidden");
        check(!Checker(parse(data + "module app depends data {\nfn main() -> int {\nreturn answer()\n}\n}\nlayer data above app\n")).check().empty(), "reverse layer dependency remains forbidden");
        auto program = parse(app("if true {\ncall id(id(2))\n}\nreturn id(3)", identity));
        std::set<NodeId> ids;
        bool unique = true;
        std::function<void(const Expr&)> expr = [&](const Expr& e) {
            if (!e) return;
            unique &= e->id != 0 && ids.insert(e->id).second;
            expr(e->left); expr(e->right);
            for (const auto& a : e->arguments) expr(a);
        };
        std::function<void(const std::vector<Statement>&)> statements = [&](const std::vector<Statement>& body) {
            for (const auto& s : body) {
                unique &= s.id != 0 && ids.insert(s.id).second;
                expr(s.expression);
                for (const auto& a : s.callArguments) expr(a);
                statements(s.body); statements(s.alternative);
                statements(s.initializer); statements(s.update);
            }
        };
        for (const auto& f : program.modules[0].functions) statements(f.body);
        check(unique, "nested statements and call expressions have unique node IDs");
        const auto semantics = SemanticAnalyzer(program).analyze();
        bool resolved = semantics.resolvedCalls.size() == 3;
        for (const auto& call : semantics.resolvedCalls) resolved &= ids.count(call.node) && semantics.findCallByNode(call.node) == &call;
        check(resolved, "nested calls resolve by stable IDs");
        check(program.modules[0].functions[1].body[0].span.endLine > program.modules[0].functions[1].body[0].span.line, "compound statements span their full bodies");
        const auto bad = parse(app("if true {\nreturn false\n}\nreturn 1"));
        const auto diagnostic = SemanticAnalyzer(bad).analyze().violations.front();
        check(diagnostic.span.file == "control.ieum" && diagnostic.span.line == 4 && diagnostic.span.column == 1, "nested return diagnostics preserve file line column");
    } catch (const std::exception& e) { check(false, e.what()); }
    parseError("return at module scope rejected", "module app {\nreturn 1\n}\n");
    parseError("standalone else rejected", app("else {}\nreturn 0"));
    parseError("unknown return type rejected", "module app {\nfn main() -> float {}\n}\n");
    parseError("incomplete call rejected", app("return id(1,)", identity));
    parseError("if needs braces", app("if true\nreturn 1"));
    std::string nested;
    for (int i = 0; i < 140; ++i) nested += "{\n";
    nested += "return 1\n";
    for (int i = 0; i < 140; ++i) nested += "}\n";
    parseError("statement nesting is bounded", app(nested));
    std::string calls = "1";
    for (int i = 0; i < 140; ++i) calls = "id(" + calls + ")";
    parseError("call expression nesting is bounded", app("return " + calls, identity));

    expect("for sum 1 through 10 is 55", app("let sum = 0\nfor (let i = 1; i <= 10; i = i + 1) {\nsum = sum + i\n}\nreturn sum"), std::int64_t{55});
    expect("for continue runs update once and unwinds nested scopes", app("let sum = 0\nfor (let i = 1; i <= 10; i = i + 1) {\n{\nlet local = i\nif local % 2 == 0 {\ncontinue\n}\n}\nsum = sum + i\n}\nreturn sum"), std::int64_t{25});
    expect("for break skips update", app("let n = 0\nfor (;; n = 1 / 0) {\n{\nbreak\n}\n}\nreturn n"), std::int64_t{0});
    expect("for return skips update and later statements", app("let n = 0\nfor (;; n = 1 / 0) {\nif true {\nreturn 7\n}\n}\nreturn 0"), std::int64_t{7});
    expect("omitted for condition means true", app("let sum = 0\nfor (let i = 1;; i = i + 1) {\nif i > 10 {\nbreak\n}\nsum = sum + i\n}\nreturn sum"), std::int64_t{55});
    expect("for allows omitted update", app("let i = 0\nfor (; i < 3;) {\ni = i + 1\ncontinue\n}\nreturn i"), std::int64_t{3});
    expect("for assignment initializer runs once", app("let i = 9\nfor (i = 0; i < 3; i = i + 1) {}\nreturn i"), std::int64_t{3});
    expect("false for condition skips body and update", app("let n = 0\nfor (n = 7; false; n = 1 / 0) {\nn = 1 / 0\n}\nreturn n"), std::int64_t{7});
    expect("for initializer shadows outer variable after evaluating it", app("let i = 8\nlet n = 0\nfor (let i = i + 1; i < 11; i = i + 1) {\nn = n + i\n}\nreturn i * 100 + n"), std::int64_t{819});
    expect("for body shadow does not replace header binding", app("let n = 0\nfor (let i = 0; i < 3; i = i + 1) {\nlet i = 100\nn = n + i\n}\nreturn n"), std::int64_t{300});
    expect("for scope removed after break", app("let i = 8\nfor (let i = 0;; i = i + 1) {\nbreak\n}\nreturn i"), std::int64_t{8});
    expect("for scope removed after zero iterations", app("let i = 8\nfor (let i = 0; false;) {}\nreturn i"), std::int64_t{8});
    expect("sibling for headers reuse names", app("let n = 0\nfor (let i = 0; i < 2; i = i + 1) {\nn = n + 1\n}\nfor (let i = 0; i < 3; i = i + 1) {\nn = n + 1\n}\nreturn n"), std::int64_t{5});
    expect("nested for break and outer continue select nearest loop", app("let n = 0\nfor (let i = 0; i < 3; i = i + 1) {\nfor (let j = 0;; j = j + 1) {\nn = n + 1\nif j == 1 {\nbreak\n}\n}\ncontinue\nn = 1 / 0\n}\nreturn n"), std::int64_t{6});
    expect("while inside for continue preserves both loop markers", app("let n = 0\nfor (let i = 0; i < 3; i = i + 1) {\nlet j = 0\nwhile j < 2 {\nj = j + 1\nn = n + 1\ncontinue\n}\n}\nreturn n"), std::int64_t{6});
    expect("for break inside while preserves outer loop", app("let i = 0\nlet n = 0\nwhile i < 3 {\ni = i + 1\nfor (;;) {\nn = n + 1\nbreak\n}\n}\nreturn n"), std::int64_t{3});
    expect("nested for return escapes all loops", app("for (let i = 0; i < 3; i = i + 1) {\nfor (;;) {\nreturn 9\n}\n}\nreturn 0"), std::int64_t{9});
    expect("for supports boolean initializer and update", app("let n = 0\nfor (let again: bool = true; again; again = false) {\nn = n + 1\n}\nreturn n"), std::int64_t{1});
    expect("unit for initializer remains compatible", app("for (let u; u == ();) {\nbreak\n}\nreturn 1"), std::int64_t{1});
    expect("for header nested call expressions", app("let n = 0\nfor (let i: int = id(1); i <= id(3); i = id(i + 1)) {\nn = n + i\n}\nreturn n", identity), std::int64_t{6});
    const std::string forCalls = "let trace = 0\nlet n = 0\nfn start() {\ntrace = trace * 10 + 1\n}\nfn condition() -> bool {\ntrace = trace * 10 + 2\nreturn n < 2\n}\nfn step() {\nn = n + 1\ntrace = trace * 10 + 4\n}\n";
    expect("for header calls run init condition body update in order", app("for (call start(); condition(); call step()) {\ntrace = trace * 10 + 3\n}\nreturn trace", forCalls), std::int64_t{12342342});
    expect("for call update runs after continue", app("for (call start(); condition(); call step()) {\ncontinue\n}\nreturn trace", forCalls), std::int64_t{124242});
    expect("for call update not run after break", app("for (call start(); condition(); call step()) {\nbreak\n}\nreturn trace", forCalls), std::int64_t{12});
    expect("call return in for update is discarded", app("for (; n < 3; call bump()) {}\nreturn n", bump), std::int64_t{3});
    runtimeError("empty for loop is step limited", app("for (;;) {}", "", "unit"), "step_limit", {75, 8});
    runtimeError("for continue loop is step limited", app("for (;;) {\ncontinue\n}", "", "unit"), "step_limit", {75, 8});
    runtimeError("for update arithmetic checked", app("for (let i = 9223372036854775807;; i = i + 1) {}\nreturn 0"), "integer_overflow");
    runtimeError("for initializer failure checked", app("for (let i = 1 / 0; false;) {}\nreturn 0"), "division_by_zero");
    runtimeError("for condition failure checked", app("for (; 1 / 0 == 0;) {}\nreturn 0"), "division_by_zero");
    runtimeError("for header calls obey depth limits", app("for (let i = id(1); false;) {}\nreturn 0", identity), "call_depth_limit", {1000, 1});
    rejects("for condition must be bool", app("for (; 1;) {}\nreturn 0"), K::TypeMismatch);
    rejects("for initializer annotation checked", app("for (let i: int = false;;) {\nbreak\n}\nreturn 0"), K::TypeMismatch);
    rejects("for update assignment type checked even if never executed", app("for (let i = 0; false; i = true) {}\nreturn 0"), K::TypeMismatch);
    rejects("for initializer cannot see body variable", app("for (let i = x; false;) {\nlet x = 1\n}\nreturn 0"), K::UndefinedVariable);
    rejects("for condition cannot see body variable", app("for (; x == 1;) {\nlet x = 1\n}\nreturn 0"), K::UndefinedVariable);
    rejects("for update cannot see body variable", app("for (; false; x = 2) {\nlet x = 1\n}\nreturn 0"), K::UndefinedVariable);
    rejects("for header variable cannot escape", app("for (let i = 0; false;) {}\nreturn i"), K::UndefinedVariable);
    rejects("for body variable cannot escape", app("for (; false;) {\nlet x = 1\n}\nreturn x"), K::UndefinedVariable);
    rejects("for still needs explicit return after loop", app("for (;;) {\nreturn 1\n}"), K::MissingReturn);
    rejects("for initializer call needs depends", data + app("for (let i = answer(); false;) {}\nreturn 0"), K::MissingCallDependency);
    rejects("for condition call needs depends", data + app("for (; answer() > 0;) {\nbreak\n}\nreturn 0"), K::MissingCallDependency);
    rejects("for update call needs depends", data + app("for (; false; call answer()) {}\nreturn 0"), K::MissingCallDependency);
    parseError("for requires parentheses", app("for let i = 0; i < 3; i = i + 1 {}\nreturn 0"));
    parseError("for requires both semicolons", app("for (let i = 0 i < 3; i = i + 1) {}\nreturn 0"));
    parseError("for update cannot declare variable", app("for (;; let i = 0) {}\nreturn 0"));
    parseError("for initializer cannot break", app("for (break;;) {}\nreturn 0"));
    parseError("for update cannot continue", app("for (;; continue) {}\nreturn 0"));
    parseError("for update cannot return", app("for (;; return 1) {}\nreturn 0"));
    parseError("for empty header still requires separators", app("for () {}\nreturn 0"));
    parseError("semicolon is not a general statement separator", app("let i = 1;\nreturn i"));
    try {
        const auto program = parse(app("for (call start(); condition(); call step()) {\ncontinue\n}\nreturn trace", forCalls));
        const auto& loop = program.modules[0].functions.back().body[0];
        const auto semantics = SemanticAnalyzer(program).analyze();
        const auto* init = semantics.findCallByNode(loop.initializer[0].id);
        const auto* condition = semantics.findCallByNode(loop.expression->id);
        const auto* update = semantics.findCallByNode(loop.update[0].id);
        check(semantics.ok() && init && condition && update &&
            init->target.function == 0 && condition->target.function == 1 && update->target.function == 2,
            "for header call nodes independently resolve their targets");
        check(loop.initializer[0].id != loop.update[0].id && loop.expression->id != loop.id &&
            loop.span.file == "control.ieum" && loop.span.endLine > loop.span.line,
            "for header IDs and full body source span preserved");
        const auto result = Interpreter(program, semantics).run("app", "main");
        check(result.success && result.entryLocals.empty() && result.callsExecuted == 6,
            "for header calls maintain trace counts without leaking locals");
    } catch (const std::exception& e) { check(false, e.what()); }

    std::cout << "Control flow tests: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
