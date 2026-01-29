#include "graph_normalizer.h"
#include <algorithm>
#include <map>

bool GraphNormalizer::is_mono_effect(const std::string& type) {
    // Effects that process a single channel
    static const std::set<std::string> mono_effects = {
        "delay",
        "gain",
        "filter",
        "distortion",
        "compressor"
    };
    return mono_effects.find(type) != mono_effects.end();
}

bool GraphNormalizer::is_stereo_aware(const std::string& type) {
    // Effects that handle stereo internally
    static const std::set<std::string> stereo_effects = {
        "input",
        "output",
        "stereo_delay",
        "panner"
    };
    return stereo_effects.find(type) != stereo_effects.end();
}

std::string GraphNormalizer::make_unique_id(const Graph& graph, const std::string& base) {
    std::string id = base;
    int counter = 1;
    while (graph.nodes.find(id) != graph.nodes.end()) {
        id = base + "_" + std::to_string(counter++);
    }
    return id;
}

void GraphNormalizer::classify_nodes(const Graph& graph,
                                      std::set<std::string>& control_nodes,
                                      std::set<std::string>& audio_nodes) {
    for (const auto& [id, node] : graph.nodes) {
        if (node.rate == SignalRate::Control) {
            control_nodes.insert(id);
        } else {
            audio_nodes.insert(id);
        }
    }
}

void GraphNormalizer::validate_signal_routing(const Graph& graph,
                                               const std::set<std::string>& control_nodes,
                                               std::vector<std::string>& errors) {
    for (const auto& conn : graph.connections) {
        bool from_is_control = control_nodes.count(conn.from.node) > 0;
        bool to_is_control = control_nodes.count(conn.to.node) > 0;

        // Check if destination is a param_input
        bool to_is_param = false;
        auto to_node_it = graph.nodes.find(conn.to.node);
        if (to_node_it != graph.nodes.end()) {
            const auto& to_node = to_node_it->second;
            for (const auto& pi : to_node.param_inputs) {
                if (pi == conn.to.port) {
                    to_is_param = true;
                    break;
                }
            }
        }

        // Validate routing rules
        if (from_is_control && !to_is_param && !to_is_control) {
            // Control output going to audio input (not param) - ERROR
            errors.push_back("Control node '" + conn.from.node +
                           "' cannot connect to audio input '" +
                           conn.to.node + ":" + conn.to.port +
                           "'. Use param_inputs for modulation.");
        }

        if (!from_is_control && to_is_param) {
            // Audio output going to param input - ERROR
            errors.push_back("Audio node '" + conn.from.node +
                           "' cannot modulate parameter '" +
                           conn.to.node + ":" + conn.to.port +
                           "'. Only control-rate signals can modulate parameters.");
        }

        if (!from_is_control && to_is_control) {
            // Audio to control - ERROR
            errors.push_back("Audio node '" + conn.from.node +
                           "' cannot connect to control node '" +
                           conn.to.node + "'.");
        }
    }
}

void GraphNormalizer::resolve_fan_ins(Graph& graph,
                                       const std::set<std::string>& control_nodes) {
    // Find all fan-ins: multiple connections to the same destination port
    // Only insert mix nodes for AUDIO fan-ins, not control/param connections
    std::map<std::string, std::vector<PortRef>> fan_ins;

    for (const auto& conn : graph.connections) {
        // Skip if source is control (these go to params, not mixed)
        if (control_nodes.count(conn.from.node) > 0) {
            continue;
        }

        // Skip if destination is a param_input
        auto to_node_it = graph.nodes.find(conn.to.node);
        if (to_node_it != graph.nodes.end()) {
            bool is_param = false;
            for (const auto& pi : to_node_it->second.param_inputs) {
                if (pi == conn.to.port) {
                    is_param = true;
                    break;
                }
            }
            if (is_param) continue;
        }

        std::string dest_key = conn.to.node + ":" + conn.to.port;
        fan_ins[dest_key].push_back(conn.from);
    }

    // Collect non-audio connections to preserve
    std::vector<GraphConnection> preserved_connections;
    for (const auto& conn : graph.connections) {
        bool from_is_control = control_nodes.count(conn.from.node) > 0;
        if (from_is_control) {
            preserved_connections.push_back(conn);
        }
    }

    // For each fan-in with multiple sources, insert a mix node
    std::vector<GraphConnection> new_connections = preserved_connections;
    int mix_counter = 0;

    for (auto& [dest_key, sources] : fan_ins) {
        if (sources.size() <= 1) {
            // No fan-in, keep original connection
            if (!sources.empty()) {
                size_t colon = dest_key.find(':');
                PortRef to_port;
                to_port.node = dest_key.substr(0, colon);
                to_port.port = dest_key.substr(colon + 1);

                GraphConnection conn;
                conn.from = sources[0];
                conn.to = to_port;
                new_connections.push_back(conn);
            }
            continue;
        }

        // Multiple sources - need a mix node
        std::string mix_id = make_unique_id(graph, "_mix_" + std::to_string(mix_counter++));

        GraphNode mix_node;
        mix_node.id = mix_id;
        mix_node.type = "mix";
        mix_node.rate = SignalRate::Audio;
        mix_node.outputs.push_back("out");

        // Create input ports for each source
        for (size_t i = 0; i < sources.size(); i++) {
            std::string in_port = "in_" + std::to_string(i);
            mix_node.inputs.push_back(in_port);

            // Connect source to mix input
            GraphConnection conn;
            conn.from = sources[i];
            conn.to.node = mix_id;
            conn.to.port = in_port;
            new_connections.push_back(conn);
        }

        // Connect mix output to original destination
        size_t colon = dest_key.find(':');
        GraphConnection conn;
        conn.from.node = mix_id;
        conn.from.port = "out";
        conn.to.node = dest_key.substr(0, colon);
        conn.to.port = dest_key.substr(colon + 1);
        new_connections.push_back(conn);

        graph.nodes[mix_id] = std::move(mix_node);
    }

    graph.connections = std::move(new_connections);
}

NormalizeResult GraphNormalizer::normalize(const Graph& input) {
    NormalizeResult result;
    result.graph = input;  // Start with a copy

    // Step 1: Classify nodes into control and audio
    classify_nodes(result.graph, result.control_nodes, result.audio_nodes);

    // Step 2: Validate signal routing
    validate_signal_routing(result.graph, result.control_nodes, result.errors);
    if (!result.errors.empty()) {
        return result;
    }

    // Step 3: Resolve audio fan-ins (insert mix nodes)
    resolve_fan_ins(result.graph, result.control_nodes);

    // Re-classify after adding mix nodes
    result.control_nodes.clear();
    result.audio_nodes.clear();
    classify_nodes(result.graph, result.control_nodes, result.audio_nodes);

    // Validate the normalized graph
    bool has_input = false;
    bool has_output = false;
    for (const auto& [id, node] : result.graph.nodes) {
        if (node.type == "input") has_input = true;
        if (node.type == "output") has_output = true;
    }

    if (!has_input) {
        result.errors.push_back("Normalized graph missing input node");
    }
    if (!has_output) {
        result.errors.push_back("Normalized graph missing output node");
    }

    result.success = result.errors.empty();
    return result;
}
