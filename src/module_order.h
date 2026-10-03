#pragma once
#include <queue>
#include <set>
#include <unordered_map>
#include "ast.h"

// Dependencies initialize before their consumers; independent modules keep source order.
inline std::vector<std::size_t> moduleInitializationOrder(const Program& program) {
    const auto count = program.modules.size();
    std::unordered_map<std::string, std::size_t> byName;
    for (std::size_t i = 0; i < count; ++i) byName.emplace(program.modules[i].name, i);
    std::vector<std::vector<std::size_t>> consumers(count);
    std::vector<std::size_t> pending(count);
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready;
    for (std::size_t i = 0; i < count; ++i) {
        std::set<std::size_t> dependencies;
        for (const auto& name : program.modules[i].deps) {
            const auto found = byName.find(name);
            if (found != byName.end()) dependencies.insert(found->second);
        }
        pending[i] = dependencies.size();
        for (auto dep : dependencies) consumers[dep].push_back(i);
        if (pending[i] == 0) ready.push(i);
    }
    std::vector<std::size_t> order;
    std::vector<bool> emitted(count, false);
    while (!ready.empty()) {
        const auto next = ready.top(); ready.pop(); order.push_back(next); emitted[next] = true;
        for (auto consumer : consumers[next]) if (--pending[consumer] == 0) ready.push(consumer);
    }
    // Checker rejects cycles before execution. Keep semantic diagnostics available for invalid ASTs.
    for (std::size_t i = 0; i < count; ++i) if (!emitted[i]) order.push_back(i);
    return order;
}
