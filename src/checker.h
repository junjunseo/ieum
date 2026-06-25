#ifndef IEUM_CHECKER_H
#define IEUM_CHECKER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "ast.h"

// ── 의존 검사기 ────────────────────────────────────────
// "이음"의 핵심: 구조 규칙을 언어 차원에서 강제한다 (끌 수 없음).
// 세 가지 규칙을 검사한다.
//   1. 암묵적 의존 금지 — 선언되지 않은 모듈에 depends 할 수 없다.
//   2. 순환 의존 금지   — 의존 관계에 사이클이 있으면 거부한다.
//   3. 계층 위반 금지   — 하위 계층이 상위 계층에 의존할 수 없다.
// 검사는 선택적이지 않다. Program이 주어지면 항상 전부 적용된다.

struct Violation {
    enum class Kind { ImplicitDependency, CyclicDependency, LayerViolation };
    Kind kind;
    std::string message;
    int line;
};

class Checker {
public:
    explicit Checker(const Program& prog) : prog_(prog) {}

    // 위반 목록을 반환한다. 비어 있으면 구조가 규칙을 만족한다는 뜻.
    std::vector<Violation> check() {
        buildIndex();
        std::vector<Violation> violations;
        checkImplicit(violations);
        checkCyclic(violations);
        checkLayer(violations);
        return violations;
    }

private:
    const Program& prog_;
    std::unordered_set<std::string> declared_;                       // 선언된 모듈 이름
    std::unordered_map<std::string, std::vector<std::string>> graph_; // 모듈 → 의존 대상
    std::unordered_map<std::string, int> moduleLine_;                 // 모듈 → 선언 행

    // ── 인덱스 구축 ────────────────────────────────────
    void buildIndex() {
        for (const auto& m : prog_.modules) {
            declared_.insert(m.name);
            graph_[m.name] = m.deps;
            moduleLine_[m.name] = m.line;
        }
    }

    // ── 규칙 1: 암묵적 의존 금지 ───────────────────────
    void checkImplicit(std::vector<Violation>& out) {
        for (const auto& m : prog_.modules) {
            for (const auto& dep : m.deps) {
                if (declared_.find(dep) == declared_.end()) {
                    out.push_back({
                        Violation::Kind::ImplicitDependency,
                        "모듈 '" + m.name + "'가 선언되지 않은 모듈 '" + dep +
                            "'에 의존합니다",
                        m.line
                    });
                }
            }
        }
    }

    // ── 규칙 2: 순환 의존 금지 (DFS 사이클 탐지) ───────
    void checkCyclic(std::vector<Violation>& out) {
        std::unordered_set<std::string> visited;   // 방문 완료
        std::unordered_set<std::string> inStack;   // 현재 DFS 경로상
        std::vector<std::string> path;

        for (const auto& m : prog_.modules) {
            if (visited.find(m.name) == visited.end()) {
                dfs(m.name, visited, inStack, path, out);
            }
        }
    }

    void dfs(const std::string& node,
             std::unordered_set<std::string>& visited,
             std::unordered_set<std::string>& inStack,
             std::vector<std::string>& path,
             std::vector<Violation>& out) {
        visited.insert(node);
        inStack.insert(node);
        path.push_back(node);

        auto it = graph_.find(node);
        if (it != graph_.end()) {
            for (const auto& dep : it->second) {
                if (declared_.find(dep) == declared_.end()) continue; // 미선언은 규칙1에서 처리
                if (inStack.find(dep) != inStack.end()) {
                    // 사이클 발견: path에서 dep부터 끝까지가 사이클
                    std::string cycle;
                    bool start = false;
                    for (const auto& n : path) {
                        if (n == dep) start = true;
                        if (start) cycle += n + " -> ";
                    }
                    cycle += dep;
                    out.push_back({
                        Violation::Kind::CyclicDependency,
                        "순환 의존 발견: " + cycle,
                        moduleLine_.count(dep) ? moduleLine_[dep] : 0
                    });
                } else if (visited.find(dep) == visited.end()) {
                    dfs(dep, visited, inStack, path, out);
                }
            }
        }

        path.pop_back();
        inStack.erase(node);
    }

    // ── 규칙 3: 계층 위반 금지 ─────────────────────────
    // layer U above L 이면, L은 U에 의존할 수 없다 (하위→상위 금지).
    void checkLayer(std::vector<Violation>& out) {
        for (const auto& layer : prog_.layers) {
            const std::string& upper = layer.upper;
            const std::string& lower = layer.lower;
            // lower 모듈의 의존 목록에 upper가 있으면 위반
            auto it = graph_.find(lower);
            if (it == graph_.end()) continue;
            for (const auto& dep : it->second) {
                if (dep == upper) {
                    out.push_back({
                        Violation::Kind::LayerViolation,
                        "계층 위반: 하위 계층 '" + lower +
                            "'가 상위 계층 '" + upper + "'에 의존합니다",
                        moduleLine_.count(lower) ? moduleLine_[lower] : layer.line
                    });
                }
            }
        }
    }
};

#endif // IEUM_CHECKER_H
