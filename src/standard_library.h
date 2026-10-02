#ifndef IEUM_STANDARD_LIBRARY_H
#define IEUM_STANDARD_LIBRARY_H

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include "ast.h"
#include "utf8.h"

inline bool isStandardModule(const std::string& name) {
    return name == "std_io" || name == "std_text" || name == "std_list";
}
inline ModuleDecl standardModule(const std::string& name) {
    ModuleDecl module;
    module.name = name; module.line = 0; module.span.file = "<" + name + ">";
    auto add = [&](const std::string& functionName, std::vector<ValueType> parameters, ValueType result) {
        FunctionDecl fn;
        fn.name = functionName; fn.line = 0; fn.span = module.span;
        fn.native = name + "." + functionName; fn.returnType = std::move(result);
        fn.parameterTypes = std::move(parameters);
        for (std::size_t i = 0; i < fn.parameterTypes.size(); ++i) {
            fn.parameters.push_back("arg" + std::to_string(i)); fn.explicitParameterTypes.push_back(true);
        }
        module.functions.push_back(std::move(fn));
    };
    if (name == "std_io") {
        add("print", {ValueType::String}, ValueType::Unit);
        add("read_line", {}, ValueType::String);
        add("read_text", {ValueType::String}, ValueType::String);
        add("write_text", {ValueType::String, ValueType::String}, ValueType::Unit);
    } else if (name == "std_text") {
        add("parse_int", {ValueType::String}, ValueType::Int);
        add("to_string", {ValueType::Int}, ValueType::String);
        add("split", {ValueType::String, ValueType::String}, ValueType::list(ValueType::String));
    } else if (name == "std_list") {
        add("length", {ValueType::list(ValueType::Unknown)}, ValueType::Int);
    }
    return module;
}
inline void installStandardLibrary(Program& program) {
    std::set<std::string> needed;
    for (const auto& module : program.modules) for (const auto& dep : module.deps) if (isStandardModule(dep)) needed.insert(dep);
    for (const auto& layer : program.layers) {
        if (isStandardModule(layer.upper)) needed.insert(layer.upper);
        if (isStandardModule(layer.lower)) needed.insert(layer.lower);
    }
    for (const auto& name : needed) program.modules.push_back(standardModule(name));
}
inline std::string standardFunctionModule(const std::string& name) {
    for (const std::string module : {"std_io", "std_text", "std_list"}) {
        for (const auto& fn : standardModule(module).functions) if (fn.name == name) return module;
    }
    return {};
}

struct RuntimeIO {
    std::istream* input = &std::cin;
    std::ostream* output = &std::cout;
};

inline Value executeStandardFunction(const std::string& name, const std::vector<Value>& args, RuntimeIO io) {
    auto stringArg = [&](std::size_t i) -> const std::string& { return std::get<std::string>(args.at(i)); };
    auto checkedText = [](std::string text) -> Value {
        if (!validUtf8(text)) throw std::runtime_error("invalid_utf8: 유효한 UTF-8 텍스트가 필요합니다");
        return text;
    };
    if (name == "std_io.print") {
        if (!io.output) throw std::runtime_error("io_error: 출력 스트림이 없습니다");
        *io.output << stringArg(0) << '\n'; io.output->flush();
        if (!*io.output) throw std::runtime_error("io_error: 출력에 실패했습니다");
        return std::monostate{};
    }
    if (name == "std_io.read_line") {
        std::string line;
        if (!io.input || !std::getline(*io.input, line)) throw std::runtime_error("io_error: 한 줄 입력을 읽을 수 없습니다 (EOF 포함)");
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return checkedText(std::move(line));
    }
    if (name == "std_io.read_text" || name == "std_io.write_text") {
        const auto& path = stringArg(0);
        if (path.empty() || path.find('\0') != std::string::npos) throw std::runtime_error("io_error: 잘못된 파일 경로입니다");
        if (name == "std_io.read_text") {
            std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
            if (!file) throw std::runtime_error("io_error: 파일을 열 수 없습니다: " + path);
            std::string text;
            char buffer[8192];
            while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) text.append(buffer, static_cast<std::size_t>(file.gcount()));
            if (file.bad()) throw std::runtime_error("io_error: 파일 읽기에 실패했습니다: " + path);
            return checkedText(std::move(text));
        }
        std::ofstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
        if (!file) throw std::runtime_error("io_error: 파일을 쓸 수 없습니다: " + path);
        const auto& text = stringArg(1);
        file.write(text.data(), static_cast<std::streamsize>(text.size())); file.flush();
        if (!file) throw std::runtime_error("io_error: 파일 쓰기에 실패했습니다: " + path);
        file.close();
        if (!file) throw std::runtime_error("io_error: 파일 닫기에 실패했습니다: " + path);
        return std::monostate{};
    }
    if (name == "std_text.parse_int") {
        const auto& text = stringArg(0);
        const auto first = text.find_first_not_of(" \t\r\n\v\f");
        const auto last = text.find_last_not_of(" \t\r\n\v\f");
        if (first == std::string::npos) throw std::runtime_error("invalid_integer: 빈 문자열은 정수가 아닙니다");
        auto begin = text.data() + first;
        const auto end = text.data() + last + 1;
        if (*begin == '+') { ++begin; if (begin == end || *begin < '0' || *begin > '9') throw std::runtime_error("invalid_integer: 정수가 필요합니다"); }
        std::int64_t number = 0;
        const auto parsed = std::from_chars(begin, end, number);
        if (parsed.ec == std::errc::result_out_of_range) throw std::runtime_error("integer_overflow: int64 범위를 초과했습니다");
        if (parsed.ec != std::errc{} || parsed.ptr != end) throw std::runtime_error("invalid_integer: 10진 정수가 필요합니다");
        return number;
    }
    if (name == "std_text.to_string") return std::to_string(std::get<std::int64_t>(args.at(0)));
    if (name == "std_text.split") {
        const auto& text = stringArg(0); const auto& separator = stringArg(1);
        if (separator.empty()) throw std::runtime_error("empty_separator: 분리 문자열은 비어 있을 수 없습니다");
        std::vector<Value> parts;
        std::size_t begin = 0;
        while (true) {
            const auto end = text.find(separator, begin);
            parts.emplace_back(text.substr(begin, end == std::string::npos ? end : end - begin));
            if (end == std::string::npos) break;
            begin = end + separator.size();
        }
        return makeList(ValueType::String, std::move(parts));
    }
    if (name == "std_list.length") return static_cast<std::int64_t>(std::get<std::shared_ptr<const ListValue>>(args.at(0))->items.size());
    throw std::runtime_error("알 수 없는 내장 함수: " + name);
}

#endif
