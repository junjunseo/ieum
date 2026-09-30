#ifndef IEUM_SOURCE_H
#define IEUM_SOURCE_H

#include <cstddef>
#include <string>

// Columns are one-based UTF-8 byte positions; end is exclusive.
struct SourceSpan {
    std::string file;
    int line = 1;
    int column = 1;
    int endLine = 1;
    int endColumn = 1;
};

using NodeId = std::size_t;

inline std::string sourceLocation(const SourceSpan& span) {
    return (span.file.empty() ? "<source>" : span.file) + ":" +
        std::to_string(span.line) + ":" + std::to_string(span.column);
}

#endif
