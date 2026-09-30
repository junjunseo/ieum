#ifndef IEUM_VALUE_H
#define IEUM_VALUE_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

enum class ValueType { Unit, Int, Bool, String };
using Value = std::variant<std::monostate, std::int64_t, bool, std::string>;

inline ValueType valueType(const Value& value) {
    return static_cast<ValueType>(value.index());
}

inline std::string typeName(ValueType type) {
    switch (type) {
        case ValueType::Int: return "int";
        case ValueType::Bool: return "bool";
        case ValueType::String: return "string";
        default: return "unit";
    }
}

inline std::string valueText(const Value& value) {
    if (auto n = std::get_if<std::int64_t>(&value)) return std::to_string(*n);
    if (auto b = std::get_if<bool>(&value)) return *b ? "true" : "false";
    if (auto s = std::get_if<std::string>(&value)) {
        std::string text = "\"";
        for (char c : *s) {
            switch (c) {
                case '\n': text += "\\n"; break;
                case '\r': text += "\\r"; break;
                case '\t': text += "\\t"; break;
                case '\\': text += "\\\\"; break;
                case '"': text += "\\\""; break;
                default: text += c;
            }
        }
        return text + "\"";
    }
    return "()";
}

// Check before performing signed arithmetic, including INT64_MIN / -1.
inline std::int64_t integerOperation(const std::string& op,
                                     std::int64_t a, std::int64_t b) {
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    bool overflow = false;
    if (op == "+") overflow = (b > 0 && a > hi - b) || (b < 0 && a < lo - b);
    else if (op == "-") overflow = (b < 0 && a > hi + b) || (b > 0 && a < lo + b);
    else if (op == "*") {
        if (a > 0) overflow = b > 0 ? a > hi / b : b < lo / a;
        else if (a < 0) overflow = b > 0 ? a < lo / b : b < 0 && a < hi / b;
    } else if (op == "/" || op == "%") {
        if (b == 0) throw std::runtime_error("division_by_zero: 0으로 나눌 수 없습니다");
        overflow = a == lo && b == -1;
    }
    if (overflow) throw std::runtime_error("integer_overflow: int64 범위를 초과했습니다");
    if (op == "+") return a + b;
    if (op == "-") return a - b;
    if (op == "*") return a * b;
    if (op == "/") return a / b;
    if (op == "%") return a % b;
    throw std::runtime_error("알 수 없는 정수 연산: " + op);
}

#endif
