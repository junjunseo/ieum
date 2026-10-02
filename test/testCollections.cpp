#include <iostream>
#include <sstream>
#include <set>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "interpreter.h"
#include "checker.h"
#include "graph.h"

static int passed = 0, failed = 0;
static void check(bool ok, const std::string& label) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << label << "\n";
    ok ? ++passed : ++failed;
}
static Program parse(const std::string& text) { return Parser(Lexer(text, "collections.ieum").tokenize()).parse(); }
static std::string app(const std::string& body, const std::string& declarations = "", const std::string& result = "int") {
    return "module app depends std_list, std_text, std_io {\n" + declarations + "fn main() -> " + result + " {\n" + body + "\n}\n}\n";
}
static ExecutionResult run(const std::string& text, RuntimeIO io = {}) {
    auto program = parse(text);
    if (!Checker(program).check().empty()) throw std::runtime_error("structural failure");
    auto semantics = SemanticAnalyzer(program).analyze();
    if (!semantics.ok()) throw std::runtime_error(semantics.violations.front().message);
    return Interpreter(program, semantics, {}, io).run("app", "main");
}
static void expect(const std::string& label, const std::string& source, const Value& value) {
    try {
        auto result = run(source);
        check(result.success && valuesEqual(result.returnValue, value), label + (result.success ? "" : ": " + result.error));
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
static void invalid(const std::string& label, const std::string& source, const std::string& message) {
    try {
        auto program = parse(source); auto semantics = SemanticAnalyzer(program).analyze();
        bool found = false;
        for (const auto& error : semantics.violations) found |= error.message.find(message) != std::string::npos;
        check(found && !Interpreter(program, semantics).run("app", "main").success, label);
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}
static void runtimeError(const std::string& label, const std::string& source, const std::string& message) {
    try {
        const auto result = run(source);
        check(!result.success && result.error.find(message) != std::string::npos && result.error.find("collections.ieum:") != std::string::npos, label);
    } catch (const std::exception& e) { check(false, label + ": " + e.what()); }
}

int main() {
    expect("list sum 60", app("let values = [10, 20, 30]\nlet sum = 0\nfor (let i = 0; i < length(values); i = i + 1) {\nsum = sum + values[i]\n}\nreturn sum"), std::int64_t{60});
    expect("list length 3", app("return length([10,20,30])"), std::int64_t{3});
    expect("empty annotated list", app("let a: list<int> = []\nreturn length(a)"), std::int64_t{0});
    expect("empty list in typed return", app("return empty() == []", "fn empty() -> list<int> {\nreturn []\n}\n", "bool"), true);
    expect("empty list in typed call argument", app("return size([])", "fn size(a: list<int>) -> int {\nreturn length(a)\n}\n"), std::int64_t{0});
    expect("empty list in assignment", app("let a = [1]\na = []\nreturn length(a)"), std::int64_t{0});
    expect("nested empty lists infer from annotation", app("let a: list<list<int>> = [[], [1]]\nreturn length(a[0]) + a[1][0]"), std::int64_t{1});
    expect("list literal and function result postfix", app("return make()[1] + [5,6][0]", "fn make() -> list<int> {\nreturn [10,20]\n}\n"), std::int64_t{25});
    expect("unary operator applies after index", app("return -[4][0] + 10"), std::int64_t{6});
    expect("list indexed update", app("let a = [10,20,30]\na[1] = 7\nreturn a[0] + a[1] + a[2]"), std::int64_t{47});
    expect("list copy isolated", app("let a = [1,2]\nlet b = a\nb[0] = 9\nreturn a[0] * 10 + b[0]"), std::int64_t{19});
    expect("nested list copy isolated", app("let a = [[1,2], [3]]\nlet b = a\nb[0][1] = 9\nreturn a[0][1] * 10 + b[0][1]"), std::int64_t{29});
    expect("parameter list update isolated", app("let a = [1]\ncall mutate(a)\nreturn a[0]", "fn mutate(a: list<int>) {\na[0] = 9\n}\n"), std::int64_t{1});
    expect("returned list update isolated from global", app("let b = get()\nb[0] = 9\nreturn a[0]", "let a = [1]\nfn get() -> list<int> {\nreturn a\n}\n"), std::int64_t{1});
    expect("list structural equality", app("return [[1], []] == [[1], []]", "", "bool"), true);
    expect("list structural inequality", app("return [1,2] != [1,3]", "", "bool"), true);
    expect("unit list", app("return length([(), ()])"), std::int64_t{2});
    expect("bool list", app("return [true, false][1]", "", "bool"), false);
    expect("UTF8 string list", app("return [\"이음\", \"언어\"][0]", "", "string"), std::string("이음"));
    const std::string person = "record Person {\nname: string\nscores: list<int>\n}\n";
    expect("record fields and constructor", app("let p = Person(\"Jun\", [10,20])\nreturn p.scores[1]", person), std::int64_t{20});
    expect("record field mutation", app("let p = Person(\"Jun\", [10])\np.name = \"이음\"\nreturn p.name", person, "string"), std::string("이음"));
    expect("record nested list copy isolated", app("let p = Person(\"Jun\", [10])\nlet q = p\nq.scores[0] = 90\nreturn p.scores[0] + q.scores[0]", person), std::int64_t{100});
    expect("list of records copy isolated", app("let p = [Person(\"Jun\", [10])]\nlet q = p\nq[0].scores[0] = 90\nreturn p[0].scores[0] + q[0].scores[0]", person), std::int64_t{100});
    expect("record nested record fields", app("let a = Box(Person(\"Jun\", [10]))\nlet b = a\nb.person.scores[0] = 5\nreturn a.person.scores[0] + b.person.scores[0]", person + "record Box {\nperson: Person\n}\n"), std::int64_t{15});
    expect("record parameter and return types", app("let p = Person(\"Jun\", [])\nlet q = update(p)\nreturn p.name + q.name", person + "fn update(p: Person) -> Person {\np.name = \"new\"\nreturn p\n}\n", "string"), std::string("Junnew"));
    expect("record equality compares contents", app("return Person(\"Jun\", [10]) == Person(\"Jun\", [10])", person, "bool"), true);
    expect("record inequality compares contents", app("return Person(\"Jun\", [10]) != Person(\"Jun\", [20])", person, "bool"), true);
    expect("empty record", app("return Empty() == Empty()", "record Empty {}\n", "bool"), true);
    expect("for header indexed assignment", app("let a = [0]\nfor (a[0] = 1; a[0] < 4; a[0] = a[0] + 1) {\ncontinue\n}\nreturn a[0]"), std::int64_t{4});
    expect("index expressions evaluated once left to right before RHS", app("let a = [[0]]\na[index()][index()] = rhs()\nreturn trace", "let trace = 0\nfn index() -> int {\ntrace = trace * 10 + 1\nreturn 0\n}\nfn rhs() -> int {\ntrace = trace * 10 + 2\nreturn 7\n}\n"), std::int64_t{112});
    expect("short circuit skips indexed failure", app("return true || [1][9] == 0", "", "bool"), true);
    runtimeError("negative index", app("return [1][-1]"), "index_out_of_range");
    runtimeError("out of bounds read", app("return [1][1]"), "index_out_of_range");
    runtimeError("out of bounds write", app("let a = [1]\na[9] = 2\nreturn 0"), "index_out_of_range");
    runtimeError("empty index", app("let a: list<int> = []\nreturn a[0]"), "index_out_of_range");
    invalid("heterogeneous list rejected", app("let a = [1, true]\nreturn 0"), "type_mismatch");
    invalid("list update wrong type rejected", app("let a = [1]\na[0] = false\nreturn 0"), "type_mismatch");
    invalid("record field wrong type rejected", app("let a = Person(\"Jun\", [])\na.name = 2\nreturn 0", person), "type_mismatch");
    invalid("missing record field rejected", app("return Person(\"Jun\", []).age", person), "unknown_field");
    invalid("scalar field access rejected", app("return 1.age"), "record_required");
    invalid("string indexing excluded", app("return \"abc\"[0]", "", "string"), "list_required");
    invalid("index must be int", app("return [1][false]"), "type_mismatch");
    invalid("empty list needs type", app("let a = []\nreturn 0"), "empty_list_type");
    invalid("length cannot infer untyped empty list", app("return length([])"), "empty_list_type");
    invalid("length only accepts list", app("return length(\"abc\")"), "list_required");
    invalid("unknown record type", app("let a: Missing = ()\nreturn 0"), "unknown_type_or_dependency");
    invalid("duplicate record fields", app("return 0", "record X {\nn: int\nn: int\n}\n"), "duplicate_field");
    invalid("duplicate records", app("return 0", "record X {}\nrecord X {}\n"), "duplicate_record");
    invalid("record self cycle", app("return 0", "record X {\nchild: X\n}\n"), "recursive_record");
    invalid("record indirect list cycle", app("return 0", "record X {\nchild: list<Y>\n}\nrecord Y {\nparent: X\n}\n"), "recursive_record");
    invalid("primitive type name cannot be record", app("return 0", "record int {}\n"), "reserved_type");
    invalid("constructor argument type checked", app("let p = Person(1, [])\nreturn 0", person), "type_mismatch");
    invalid("nominal record types are distinct", app("let a = A(1)\na = B(1)\nreturn 0", "record A {\nn: int\n}\nrecord B {\nn: int\n}\n"), "type_mismatch");
    invalid("builtin depends required", "module app {\nfn main() {\ncall print(\"hi\")\n}\n}\n", "depends std_io");
    const std::string data = "module data {\nrecord Point {\nx: int\n}\n}\n";
    expect("dependent module record type and constructor", data + "module app depends data {\nfn main() -> int {\nlet p: Point = Point(8)\nreturn p.x\n}\n}\n", std::int64_t{8});
    invalid("constructor depends required", data + "module app {\nfn main() {\nlet p = Point(8)\n}\n}\n", "depends");
    invalid("record annotation depends required", data + "module app {\nfn main(p: Point) {}\n}\n", "unknown_type_or_dependency");
    const std::string other = "module other {\nrecord Point {\nx: int\n}\n}\n";
    invalid("ambiguous record type", data + other + "module app depends data, other {\nfn main(p: Point) {}\n}\n", "ambiguous_type");
    std::string deepRecords = "record R1000 {}\n";
    for (int i = 1001; i < 1128; ++i) deepRecords += "record R" + std::to_string(i) + " {\nchild: R" + std::to_string(i - 1) + "\n}\n";
    expect("128 record levels accepted", app("return 0", deepRecords), std::int64_t{0});
    deepRecords += "record R1128 {\nchild: R1127\n}\n";
    invalid("record depth limit includes memoized earlier types", app("return 0", deepRecords), "recursive_record");
    std::string nestedValues = "let v0 = [0]\n";
    for (int i = 1; i < 129; ++i) nestedValues += "let v" + std::to_string(i) + " = [v" + std::to_string(i - 1) + "]\n";
    invalid("inferred list depth is bounded", app(nestedValues + "return 0"), "type_depth_limit");
    expect("split and parse sum", app("let parts = split(\"10,20,30\", \",\")\nlet sum = 0\nfor (let i = 0; i < length(parts); i = i + 1) {\nsum = sum + parse_int(parts[i])\n}\nreturn sum"), std::int64_t{60});
    expect("split preserves empty parts", app("let parts = split(\",a,,\", \",\")\nreturn length(parts)"), std::int64_t{4});
    expect("split UTF8 delimiter", app("return split(\"앞이음뒤\", \"이음\")[1]", "", "string"), std::string("뒤"));
    expect("split empty text yields one empty part", app("return length(split(\"\", \",\"))"), std::int64_t{1});
    expect("parse integer whitespace and plus", app("return parse_int(\" \\t+30\\r\\n\")"), std::int64_t{30});
    expect("parse int64 minimum", app("return parse_int(\"-9223372036854775808\")"), std::numeric_limits<std::int64_t>::min());
    expect("explicit integer to string", app("return to_string(-120)", "", "string"), std::string("-120"));
    runtimeError("parse integer overflow", app("return parse_int(\"9223372036854775808\")"), "integer_overflow");
    runtimeError("parse integer garbage rejected", app("return parse_int(\"12x\")"), "invalid_integer");
    runtimeError("parse empty string rejected", app("return parse_int(\" \" )"), "invalid_integer");
    runtimeError("parse double sign rejected", app("return parse_int(\"+-1\")"), "invalid_integer");
    runtimeError("empty split separator rejected", app("return length(split(\"abc\", \"\"))"), "empty_separator");
    try {
        std::istringstream input("안녕\r\n\nlast"); std::ostringstream output;
        auto result = run(app("call print(read_line())\nlet blank = read_line()\nreturn read_line()", "", "string"), {&input, &output});
        check(result.success && std::get<std::string>(result.returnValue) == "last" && output.str() == "안녕\n", "injected UTF8 line input and output");
        check(std::get<std::string>(result.entryLocals.at("blank")).empty(), "blank input line preserved");
        result = run(app("return read_line()", "", "string"), {&input, &output});
        check(!result.success && result.error.find("io_error") != std::string::npos, "EOF is diagnosed");
        std::istringstream invalidInput(std::string("\xC0\xAF\n"));
        result = run(app("return read_line()", "", "string"), {&invalidInput, &output});
        check(!result.success && result.error.find("invalid_utf8") != std::string::npos, "invalid UTF8 input rejected");
        output.setstate(std::ios::badbit);
        result = run(app("call print(\"x\")\nreturn 0"), {&input, &output});
        check(!result.success && result.error.find("io_error") != std::string::npos, "output stream failure diagnosed");
        auto plain = parse("module app\n");
        check(plain.modules.size() == 1 && DependencyGraphExporter::toDot(plain, {}).find("std_") == std::string::npos, "unused builtins do not alter old graph");
        auto imported = parse("module app depends std_list\nlayer app above std_list\n");
        check(imported.modules.size() == 2 && Checker(imported).check().empty() && DependencyGraphExporter::toDot(imported, {}).find("std_list") != std::string::npos, "used builtin modules participate in graph");
        auto reversed = parse("module app depends std_list\nlayer std_list above app\n");
        check(!Checker(reversed).check().empty(), "builtin reverse layer dependency rejected");
        check(valueText(makeList(ValueType::Int, {std::int64_t{1}, std::int64_t{2}})) == "[1, 2]", "list display deterministic");
        auto record = run(app("return Person(\"Jun\", [10])", person, "Person"));
        check(record.success && valueText(record.returnValue) == "app.Person{name: \"Jun\", scores: [10]}", "record display preserves declaration order");
    } catch (const std::exception& e) { check(false, e.what()); }
    std::cout << "Collections tests: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
