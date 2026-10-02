#ifndef IEUM_VALUE_H
#define IEUM_VALUE_H

#include <cstdint>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>
#include <memory>
#include <vector>
#include <utility>

struct ValueType {
    enum Kind { Unit, Int, Bool, String, List, Record, Unknown };
    Kind kind;
    std::shared_ptr<const ValueType> element;
    std::string name;
    ValueType(Kind k = Unit) : kind(k) {}
    static ValueType list(ValueType item) {
        ValueType t(List); t.element = std::make_shared<const ValueType>(std::move(item)); return t;
    }
    static ValueType record(std::string name) {
        ValueType t(Record); t.name = std::move(name); return t;
    }
    bool operator==(const ValueType& other) const {
        if (kind != other.kind) return false;
        if (kind == Record) return name == other.name;
        if (kind == List) return element && other.element && *element == *other.element;
        return true;
    }
    bool operator!=(const ValueType& other) const { return !(*this == other); }
};
struct ListValue;
struct RecordValue;
// Aggregates are immutable. Updating a path builds new aggregate nodes, preserving value semantics.
using Value = std::variant<std::monostate, std::int64_t, bool, std::string,
    std::shared_ptr<const ListValue>, std::shared_ptr<const RecordValue>>;
struct ListValue { ValueType elementType; std::vector<Value> items; std::size_t depth = 1; };
struct RecordValue { std::string name; std::vector<std::pair<std::string, Value>> fields; std::size_t depth = 1; };
inline std::size_t valueDepth(const Value& value) {
    if (auto list = std::get_if<std::shared_ptr<const ListValue>>(&value)) return (*list)->depth;
    if (auto record = std::get_if<std::shared_ptr<const RecordValue>>(&value)) return (*record)->depth;
    return 0;
}
inline Value makeList(ValueType element, std::vector<Value> items) {
    std::size_t depth = 1;
    for (const auto& item : items) depth = std::max(depth, 1 + valueDepth(item));
    if (depth > 128) throw std::runtime_error("value_depth_limit: 자료구조 최대 중첩 깊이를 초과했습니다");
    return std::make_shared<const ListValue>(ListValue{std::move(element), std::move(items), depth});
}
inline Value makeRecord(std::string name, std::vector<std::pair<std::string, Value>> fields) {
    std::size_t depth = 1;
    for (const auto& field : fields) depth = std::max(depth, 1 + valueDepth(field.second));
    if (depth > 128) throw std::runtime_error("value_depth_limit: 자료구조 최대 중첩 깊이를 초과했습니다");
    return std::make_shared<const RecordValue>(RecordValue{std::move(name), std::move(fields), depth});
}
inline ValueType valueType(const Value& value) {
    if (auto list = std::get_if<std::shared_ptr<const ListValue>>(&value)) return ValueType::list((*list)->elementType);
    if (auto record = std::get_if<std::shared_ptr<const RecordValue>>(&value)) return ValueType::record((*record)->name);
    return ValueType(static_cast<ValueType::Kind>(value.index()));
}
inline std::string typeName(const ValueType& type) {
    switch (type.kind) {
        case ValueType::Int: return "int";
        case ValueType::Bool: return "bool";
        case ValueType::String: return "string";
        case ValueType::List: return "list<" + typeName(*type.element) + ">";
        case ValueType::Record: return type.name;
        case ValueType::Unknown: return "?";
        default: return "unit";
    }
}
inline bool valuesEqual(const Value& left, const Value& right) {
    if (valueType(left) != valueType(right)) return false;
    if (auto list = std::get_if<std::shared_ptr<const ListValue>>(&left)) {
        const auto& a = (*list)->items;
        const auto& b = std::get<std::shared_ptr<const ListValue>>(right)->items;
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (!valuesEqual(a[i], b[i])) return false;
        return true;
    }
    if (auto record = std::get_if<std::shared_ptr<const RecordValue>>(&left)) {
        const auto& a = (*record)->fields;
        const auto& b = std::get<std::shared_ptr<const RecordValue>>(right)->fields;
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (a[i].first != b[i].first || !valuesEqual(a[i].second, b[i].second)) return false;
        return true;
    }
    return left == right;
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
    if (auto list = std::get_if<std::shared_ptr<const ListValue>>(&value)) {
        std::string text = "[";
        for (const auto& item : (*list)->items) { if (text.size() > 1) text += ", "; text += valueText(item); }
        return text + "]";
    }
    if (auto record = std::get_if<std::shared_ptr<const RecordValue>>(&value)) {
        std::string text = (*record)->name + "{";
        bool first = true;
        for (const auto& field : (*record)->fields) {
            if (!first) text += ", ";
            first = false; text += field.first + ": " + valueText(field.second);
        }
        return text + "}";
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
