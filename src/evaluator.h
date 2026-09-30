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

inline const Value& lookupValue(const std::string& name, const ValueScope& locals,
                                const ValueScope& module, const SourceSpan& span) {
    const auto local = locals.find(name);
    if (local != locals.end()) return local->second;
    const auto global = module.find(name);
    if (global != module.end()) return global->second;
    throw EvaluationError(span, "초기화된 변수를 찾을 수 없습니다: " + name);
}

inline Value evaluateExpression(const Expr& expr, const ValueScope& locals,
                                const ValueScope& module) {
    if (!expr) return std::monostate{};
    try {
        if (expr->kind == Expression::Kind::Literal) return expr->literal;
        if (expr->kind == Expression::Kind::Name) return lookupValue(expr->text, locals, module, expr->span);
        const auto& op = expr->text;
        if (expr->kind == Expression::Kind::Unary) {
            const Value right = evaluateExpression(expr->right, locals, module);
            if (op == "!") return !std::get<bool>(right);
            const auto n = std::get<std::int64_t>(right);
            return op == "-" ? integerOperation("-", 0, n) : n;
        }
        const Value left = evaluateExpression(expr->left, locals, module);
        if (op == "&&" && !std::get<bool>(left)) return false;
        if (op == "||" && std::get<bool>(left)) return true;
        const Value right = evaluateExpression(expr->right, locals, module);
        if (op == "&&" || op == "||") return std::get<bool>(right);
        if (op == "==") return left == right;
        if (op == "!=") return left != right;
        if (op == "+" && std::holds_alternative<std::string>(left)) return std::get<std::string>(left) + std::get<std::string>(right);
        const auto a = std::get<std::int64_t>(left), b = std::get<std::int64_t>(right);
        if (op == "<") return a < b;
        if (op == "<=") return a <= b;
        if (op == ">") return a > b;
        if (op == ">=") return a >= b;
        return integerOperation(op, a, b);
    } catch (const EvaluationError&) {
        throw;
    } catch (const std::exception& error) {
        throw EvaluationError(expr->span, error.what());
    }
}

#endif
