#pragma once

#include "graph_ast.h"
#include <string>
#include <vector>

// Result of normalization
struct NormalizeResult {
    bool success = false;
    Graph graph;  // Normalized graph
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// Graph Normalizer - Phase 6
// Transforms a parsed Graph AST into a normalized form ready for compilation:
// - Expands channels (mono effects receiving stereo become duplicated)
// - Resolves fan-ins (multiple connections to one port get a mix node)
// - Validates graph connectivity
class GraphNormalizer {
public:
    static NormalizeResult normalize(const Graph& input);

private:
    // Check if a node type is inherently mono (processes single channel)
    static bool is_mono_effect(const std::string& type);

    // Check if a node type handles stereo internally
    static bool is_stereo_aware(const std::string& type);

    // Insert mix nodes where multiple connections feed one input
    static void resolve_fan_ins(Graph& graph);

    // Expand mono effects to handle stereo by duplication
    static void expand_channels(Graph& graph);

    // Generate unique node ID
    static std::string make_unique_id(const Graph& graph, const std::string& base);
};
