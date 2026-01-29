#pragma once

#include "graph_ast.h"
#include <string>
#include <vector>
#include <set>

// Result of normalization
struct NormalizeResult {
    bool success = false;
    Graph graph;  // Normalized graph

    // Classified node sets
    std::set<std::string> control_nodes;  // Nodes that run at control rate
    std::set<std::string> audio_nodes;    // Nodes that run at audio rate

    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// Graph Normalizer - Phase 6
// Transforms a parsed Graph AST into a normalized form ready for compilation:
// - Classifies nodes into control and audio subgraphs
// - Resolves fan-ins (multiple connections to one port get a mix node)
// - Validates:
//   - No audio -> control connections
//   - Control outputs only connect to param_inputs
class GraphNormalizer {
public:
    static NormalizeResult normalize(const Graph& input);

private:
    // Check if a node type is inherently mono (processes single channel)
    static bool is_mono_effect(const std::string& type);

    // Check if a node type handles stereo internally
    static bool is_stereo_aware(const std::string& type);

    // Classify nodes into control and audio sets
    static void classify_nodes(const Graph& graph,
                               std::set<std::string>& control_nodes,
                               std::set<std::string>& audio_nodes);

    // Validate control/audio signal routing
    static void validate_signal_routing(const Graph& graph,
                                        const std::set<std::string>& control_nodes,
                                        std::vector<std::string>& errors);

    // Insert mix nodes where multiple connections feed one input
    static void resolve_fan_ins(Graph& graph,
                                const std::set<std::string>& control_nodes);

    // Generate unique node ID
    static std::string make_unique_id(const Graph& graph, const std::string& base);
};
