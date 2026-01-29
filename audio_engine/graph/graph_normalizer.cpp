#include "graph_normalizer.h"
#include <algorithm>
#include <map>
#include <set>

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

void GraphNormalizer::resolve_fan_ins(Graph& graph) {
    // Find all fan-ins: multiple connections to the same destination port
    // Map: "node:port" -> list of source PortRefs
    std::map<std::string, std::vector<PortRef>> fan_ins;

    for (const auto& conn : graph.connections) {
        std::string dest_key = conn.to.node + ":" + conn.to.port;
        fan_ins[dest_key].push_back(conn.from);
    }

    // For each fan-in with multiple sources, insert a mix node
    std::vector<GraphConnection> new_connections;
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

void GraphNormalizer::expand_channels(Graph& graph) {
    // For each mono effect, check if it receives from multiple channels
    // If so, duplicate the effect for each channel

    // Build a map of what channels feed into each mono effect
    std::map<std::string, std::set<std::string>> node_input_channels;

    for (const auto& conn : graph.connections) {
        // Track which "channel context" each input comes from
        // For now, we use the port name as the channel identifier
        auto it = graph.nodes.find(conn.to.node);
        if (it != graph.nodes.end() && is_mono_effect(it->second.type)) {
            // This is a connection to a mono effect
            // The channel is determined by the source port
            node_input_channels[conn.to.node].insert(conn.from.port);
        }
    }

    // For mono effects receiving multiple channels, duplicate them
    std::vector<std::string> nodes_to_expand;
    for (const auto& [node_id, channels] : node_input_channels) {
        if (channels.size() > 1) {
            nodes_to_expand.push_back(node_id);
        }
    }

    for (const std::string& node_id : nodes_to_expand) {
        auto& original = graph.nodes.at(node_id);
        const auto& channels = node_input_channels.at(node_id);

        // Create a copy for each channel
        std::map<std::string, std::string> channel_to_node;  // channel -> new node id

        bool first = true;
        for (const std::string& channel : channels) {
            std::string new_id;
            if (first) {
                // Reuse original node for first channel
                new_id = node_id;
                first = false;
            } else {
                // Create duplicate
                new_id = make_unique_id(graph, node_id + "_" + channel);
                GraphNode copy = original;
                copy.id = new_id;
                graph.nodes[new_id] = copy;
            }
            channel_to_node[channel] = new_id;
        }

        // Rewire connections
        std::vector<GraphConnection> updated_connections;
        for (auto& conn : graph.connections) {
            if (conn.to.node == node_id) {
                // Redirect to the channel-specific node
                auto it = channel_to_node.find(conn.from.port);
                if (it != channel_to_node.end()) {
                    conn.to.node = it->second;
                }
            }
            if (conn.from.node == node_id) {
                // Duplicate outgoing connections for each channel node
                for (const auto& [channel, new_node_id] : channel_to_node) {
                    GraphConnection new_conn = conn;
                    new_conn.from.node = new_node_id;
                    // Only add if this matches the expected output channel
                    // For simplicity, add all - the topo sort will handle unused paths
                    updated_connections.push_back(new_conn);
                }
                continue;  // Don't add original
            }
            updated_connections.push_back(conn);
        }
        graph.connections = std::move(updated_connections);
    }
}

NormalizeResult GraphNormalizer::normalize(const Graph& input) {
    NormalizeResult result;
    result.graph = input;  // Start with a copy

    // Step 1: Resolve fan-ins (insert mix nodes)
    resolve_fan_ins(result.graph);

    // Step 2: Expand channels for mono effects
    // Note: For now, we keep this simple - mono effects receiving multiple
    // channels will sum them (via the fan-in mix). True channel expansion
    // would require tracking channel context through the graph.
    // expand_channels(result.graph);  // Disabled for now - use explicit L/R nodes

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
