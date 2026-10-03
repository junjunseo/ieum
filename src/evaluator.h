#ifndef IEUM_EVALUATOR_H
#define IEUM_EVALUATOR_H

#include <unordered_map>
#include "ast.h"

using ValueScope = std::unordered_map<std::string, Value>;

class EvaluationError : public SourceError {
public:
    EvaluationError(const SourceSpan& span, const std::string& message)
        : SourceError(span, message) {}
};

inline Value unaryValue(const std::string& op, const Value& right) {
    if (op == "!") return !std::get<bool>(right);
    const auto n = std::get<std::int64_t>(right);
    return op == "-" ? integerOperation("-", 0, n) : n;
}

inline Value binaryValue(const std::string& op, const Value& left, const Value& right) {
    if (op == "==") return valuesEqual(left, right);
    if (op == "!=") return !valuesEqual(left, right);
    if (op == "+" && std::holds_alternative<std::string>(left)) return std::get<std::string>(left) + std::get<std::string>(right);
    const auto a = std::get<std::int64_t>(left), b = std::get<std::int64_t>(right);
    if (op == "<") return a < b;
    if (op == "<=") return a <= b;
    if (op == ">") return a > b;
    if (op == ">=") return a >= b;
    return integerOperation(op, a, b);
}

inline const Value& indexValue(const Value& value, std::int64_t index) {
    const auto& items = std::get<std::shared_ptr<const ListValue>>(value)->items;
    if (index < 0 || static_cast<std::uint64_t>(index) >= items.size()) throw std::runtime_error("index_out_of_range: 목록 인덱스가 범위를 벗어났습니다");
    return items[static_cast<std::size_t>(index)];
}
inline const Value& fieldValue(const Value& value, const std::string& name) {
    for (const auto& field : std::get<std::shared_ptr<const RecordValue>>(value)->fields) if (field.first == name) return field.second;
    throw std::runtime_error("unknown_field: 필드를 찾을 수 없습니다: " + name);
}

#endif
