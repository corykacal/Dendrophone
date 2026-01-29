#pragma once

#include "graph_ast.h"
#include <string>
#include <optional>
#include <vector>

// Result of parsing/validation with error messages
struct ParseResult {
    bool success = false;
    Graph graph;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// .dpt file parser
// Parses JSON into Graph AST, validates structure
class DptParser {
public:
    static constexpr int CURRENT_VERSION = 1;
    static constexpr int MIN_SUPPORTED_VERSION = 1;

    // Parse .dpt JSON string into Graph AST
    static ParseResult parse(const std::string& json_str);

    // Parse .dpt file from path
    static ParseResult parse_file(const std::string& path);

    // Validate a parsed graph
    static std::vector<std::string> validate(const Graph& graph);

private:
    static PortRef parse_port_ref(const std::string& ref);
};
