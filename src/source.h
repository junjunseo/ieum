#ifndef IEUM_SOURCE_H
#define IEUM_SOURCE_H

#include <cstddef>
#include <string>
#include <stdexcept>
#include <map>
#include <sstream>
#include <utility>

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

class SourceError : public std::runtime_error {
public:
    SourceSpan span;
    SourceError(SourceSpan location, const std::string& message)
        : std::runtime_error(sourceLocation(location) + " " + message), span(std::move(location)) {}
};

inline std::string sourceContext(const std::map<std::string, std::string>& sources, const SourceSpan& span) {
    const auto found = sources.find(span.file);
    if (found == sources.end() || span.line < 1) return {};
    std::istringstream input(found->second);
    std::string line;
    for (int n = 1; n <= span.line; ++n) if (!std::getline(input, line)) return {};
    if (span.line == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // Preserve tabs; columns in diagnostics remain one-based UTF-8 byte offsets.
    std::string padding;
    for (std::size_t i = 0; i < line.size() && i + 1 < static_cast<std::size_t>(span.column); ++i) {
        const auto c = static_cast<unsigned char>(line[i]);
        if (c == '\t') padding += '\t';
        else if ((c & 0xC0) != 0x80) padding += ' ';
    }
    return "    " + line + "\n    " + padding + "^\n";
}

#endif
