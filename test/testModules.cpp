#include <iostream>
#include <sstream>
#include "lexer.h"
#include "parser.h"
#include "checker.h"
#include "semantic.h"
#include "interpreter.h"

static int passed = 0, failed = 0;
static void check(bool ok, const std::string& label) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << label << "\n";
    ok ? ++passed : ++failed;
}
static Program parse(const std::string& source) { return Parser(Lexer(source, "modules.ieum").tokenize()).parse(); }
static std::string app(const std::string& body, const std::string& deps = "data", const std::string& result = "int") {
    return "module app" + (deps.empty() ? std::string{} : " depends " + deps) + " {\nfn main() -> " + result + " {\n" + body + "\n}\n}\n";
}
static const std::string data = "module data {\nlet value = 7\nlet values = [10,20,30]\nprivate let secret = 99\nrecord Point {\nx: int\n}\nprivate record Hidden {\nx: int\n}\nfn get() -> int {\nreturn value\n}\nprivate fn hidden() -> int {\nreturn secret\n}\nfn public_get() -> int {\nreturn hidden()\n}\n}\n";
static void expect(const std::string& label, const std::string& source, std::int64_t expected) {
    try {
        auto p = parse(source); auto structural = Checker(p).check(); auto s = SemanticAnalyzer(p).analyze();
        if (!structural.empty()) throw std::runtime_error(structural.front().message);
        if (!s.ok()) throw std::runtime_error(s.violations.front().message);
        auto r = Interpreter(p, s).run("app", "main");
        check(r.success && std::get<std::int64_t>(r.returnValue) == expected, label + (r.success ? "" : ": " + r.error));
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
static void invalid(const std::string& label, const std::string& source, const std::string& code) {
    try {
        auto p = parse(source); auto s = SemanticAnalyzer(p).analyze(); bool found = false;
        for (const auto& error : s.violations) found |= error.message.find(code) != std::string::npos;
        check(found && !Interpreter(p, s).run("app", "main").success, label);
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
int main() {
    expect("qualified call", data + app("return data.get()"), 7);
    expect("qualified call statement", data + app("call data.get()\nreturn 1"), 1);
    expect("qualified module value", data + app("return data.value"), 7);
    expect("qualified module value assignment", data + app("data.value = 12\nreturn data.get()"), 12);
    expect("qualified module indexed assignment", data + app("data.values[1] = 8\nreturn data.values[1]"), 8);
    expect("module list value copy isolated", data + app("let a = data.values\na[0] = 8\nreturn data.values[0]"), 10);
    expect("qualified constructor and type", data + app("let p: data.Point = data.Point(5)\np.x = 8\nreturn p.x"), 8);
    expect("qualified call postfix", data + app("return data.Point(6).x"), 6);
    expect("private allowed within owner", data + app("return data.public_get()"), 99);
    expect("unqualified imports retained", data + app("return get()"), 7);
    expect("qualified self module", "module app {\nlet n = 3\nprivate fn f() -> int {\nreturn app.n\n}\nfn main() -> int {\nreturn app.f()\n}\n}\n", 3);
    const auto initialized = "module app depends data {\nlet answer = data.value + 1\nfn main() -> int {\nreturn answer\n}\n}\n";
    expect("dependency globals before consumer source", initialized + data, 8);
    expect("for header qualified target", data + app("for (data.value = 0; data.value < 3; data.value = data.value + 1) {\ncontinue\n}\nreturn data.value"), 3);
    expect("for header qualified call", data + app("for (call data.get(); false; call data.get()) {}\nreturn 1"), 1);
    expect("nested call and list qualified references", data + app("let xs = [data.get(), data.Point(data.value).x]\nreturn xs[0] + xs[1]"), 14);
    const auto other = "module other {\nfn get() -> int {\nreturn 2\n}\nrecord Point {\nx: int\n}\n}\n";
    expect("qualified resolves function collision", data + other + app("return data.get() + other.get()", "data, other"), 9);
    expect("qualified resolves type collision", data + other + app("let p: other.Point = other.Point(4)\nreturn p.x", "data, other"), 4);
    const auto hiddenOther = "module other {\nprivate fn get() -> int {\nreturn 0\n}\nprivate record Point {}\n}\n";
    expect("private function does not hide public import", data + hiddenOther + app("return get()", "data, other"), 7);
    expect("private type does not hide public import", data + hiddenOther + app("let p: Point = Point(4)\nreturn p.x", "data, other"), 4);
    invalid("qualified builtin requires dependency", app("call std_io.print(\"x\")\nreturn 0", ""), "missing_dependency");
    invalid("unqualified collision remains ambiguous", data + other + app("return get()", "data, other"), "여러 의존 모듈");
    invalid("qualified call requires depends", data + app("return data.get()", ""), "missing_dependency");
    invalid("call statement requires depends", data + app("call data.get()\nreturn 1", ""), "missing_dependency");
    invalid("qualified value requires depends", data + app("return data.value", ""), "missing_dependency");
    invalid("qualified write requires depends", data + app("data.value = 1\nreturn 0", ""), "missing_dependency");
    invalid("qualified constructor requires depends", data + app("return data.Point(1).x", ""), "missing_dependency");
    invalid("qualified type requires depends", data + app("let p: data.Point = ()\nreturn 0", ""), "missing_dependency");
    invalid("private function qualified", data + app("return data.hidden()"), "private_access");
    invalid("private function unqualified", data + app("return hidden()"), "private_access");
    invalid("private module value", data + app("return data.secret"), "private_access");
    invalid("private module value write", data + app("data.secret = 1\nreturn 0"), "private_access");
    invalid("private constructor", data + app("return data.Hidden(1).x"), "private_access");
    invalid("private type", data + app("let p: data.Hidden = ()\nreturn 0"), "private_access");
    invalid("private unqualified type", data + app("let p: Hidden = ()\nreturn 0"), "private_access");
    invalid("missing qualified function", data + app("return data.missing()"), "undefined_function");
    invalid("missing module value", data + app("return data.missing"), "undefined_module_value");
    invalid("missing qualified module", app("return missing.get()", ""), "unknown_module");
    invalid("missing type", data + app("let p: data.Missing = ()\nreturn 0"), "unknown_type");
    invalid("qualified assignment type checked", data + app("data.value = true\nreturn 0"), "type_mismatch");
    invalid("qualified call in for update checked", data + app("for (;; call data.hidden()) {\nbreak\n}\nreturn 0"), "private_access");
    invalid("qualified index in for header checked", data + app("for (data.values[data.hidden()] = 0; false;) {}\nreturn 0"), "private_access");
    const auto wrapper = "module service depends data {\nfn point() -> data.Point {\nreturn data.Point(1)\n}\n}\n";
    invalid("returned record requires type owner dependency", data + wrapper + app("return service.point().x", "service"), "missing_dependency");
    const auto takesPoints = "module service depends data {\nfn take(points: list<data.Point>) -> int {\nreturn 0\n}\n}\n";
    invalid("empty list parameter context requires type dependency", data + takesPoints + app("return service.take([])", "service"), "missing_dependency");
    expect("empty list parameter with type dependency", data + takesPoints + app("return service.take([])", "service, data"), 0);
    expect("returned record with explicit type dependency", data + wrapper + app("return service.point().x", "service, data"), 1);
    invalid("private type cannot leak through public return", "module data {\nprivate record Hidden {}\nfn expose() -> Hidden {\nreturn Hidden()\n}\n}\n" + app("let h = data.expose()\nreturn 0"), "private_access");
    try {
        auto p = parse("module app {\nprivate fn main() {}\n}\n"); auto s = SemanticAnalyzer(p).analyze();
        auto r = Interpreter(p, s).run("app", "main");
        check(!r.success && r.error.find("private_access") != std::string::npos, "private entry rejected");
        p = parse("module data {\nfn fail() -> int {\nreturn 1 / 0\n}\n}\n" + app("return data.fail()"));
        s = SemanticAnalyzer(p).analyze(); r = Interpreter(p, s).run("app", "main");
        check(!r.success && r.failureSpan.line == 3 && r.failureSpan.file == "modules.ieum", "runtime source span retained");
        check(r.callStack.size() == 2 && r.callStack[0].first == "data.fail" && r.callStack[1].first == "app.main", "runtime stack innermost first");
        p = parse(data + app("return data.value") + "layer data above app\n");
        check(!Checker(p).check().empty(), "qualified references obey layers");
        p = parse("module data depends app\n" + app("return 1"));
        check(!Checker(p).check().empty(), "module cycle remains rejected");
        p = parse(data + app("let data = Point(3)\nreturn data.x")); s = SemanticAnalyzer(p).analyze(); r = Interpreter(p,s).run("app","main");
        check(r.success && std::get<std::int64_t>(r.returnValue) == 3, "local record shadows module value prefix");
    } catch (const std::exception& e) { check(false, e.what()); }
    std::cout << "Module tests: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
