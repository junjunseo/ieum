#ifndef IEUM_EVALUATOR_H
#define IEUM_EVALUATOR_H

#include <unordered_map>
#include "ast.h"

using ValueScope = std::unordered_map<std::string, Value>;

class EvaluationError : public std::runtime_error {
public:
    EvaluationError(const SourceSpan& span, const std::string& message)
        : std::runtime_error(sourceLocation(span) + " " + message) {}
};

inline Value unaryValue(const std::string& op, const Value& right) {
    if (op == "!") return !std::get<bool>(right);
    const auto n = std::get<std::int64_t>(right);
    return op == "-" ? integerOperation("-", 0, n) : n;
}

inline Value binaryValue(const std::string& op, const Value& left, const Value& right) {
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    if (op == "+" && std::holds_alternative<std::string>(left)) return std::get<std::string>(left) + std::get<std::string>(right);
    const auto a = std::get<std::int64_t>(left), b = std::get<std::int64_t>(right);
    if (op == "<") return a < b;
    if (op == "<=") return a <= b;
    if (op == ">") return a > b;
    if (op == ">=") return a >= b;
    return integerOperation(op, a, b);
}

#endif
