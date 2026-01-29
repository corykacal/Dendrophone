#include "dpt_parser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <set>

using json = nlohmann::json;

PortRef DptParser::parse_port_ref(const std::string& ref) {
    PortRef result;
    size_t colon = ref.find(':');
    if (colon == std::string::npos) {
        result.node = ref;
        result.port = "";
    } else {
        result.node = ref.substr(0, colon);
        result.port = ref.substr(colon + 1);
    }
    return result;
}

ParseResult DptParser::parse(const std::string& json_str) {
    ParseResult result;

    // Parse JSON
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        result.errors.push_back(std::string("JSON parse error: ") + e.what());
        return result;
    }

    // Version check
    if (!j.contains("dpt_version")) {
        result.errors.push_back("Missing 'dpt_version' field");
        return result;
    }

    result.graph.version = j["dpt_version"].get<int>();

    if (result.graph.version < MIN_SUPPORTED_VERSION) {
        result.errors.push_back("Version " + std::to_string(result.graph.version) +
                                " is too old (minimum: " + std::to_string(MIN_SUPPORTED_VERSION) + ")");
        return result;
    }

    if (result.graph.version > CURRENT_VERSION) {
        result.warnings.push_back("Version " + std::to_string(result.graph.version) +
                                  " is newer than supported (" + std::to_string(CURRENT_VERSION) +
                                  "), some features may be ignored");
    }

    // Parse audio config
    if (j.contains("audio")) {
        auto& audio = j["audio"];
        if (audio.contains("inputs")) {
            for (const auto& inp : audio["inputs"]) {
                result.graph.audio_inputs.push_back(inp.get<std::string>());
            }
        }
        if (audio.contains("outputs")) {
            for (const auto& out : audio["outputs"]) {
                result.graph.audio_outputs.push_back(out.get<std::string>());
            }
        }
    }

    // Default to stereo if not specified
    if (result.graph.audio_inputs.empty()) {
        result.graph.audio_inputs = {"L", "R"};
        result.warnings.push_back("No audio inputs specified, defaulting to L/R stereo");
    }
    if (result.graph.audio_outputs.empty()) {
        result.graph.audio_outputs = {"L", "R"};
        result.warnings.push_back("No audio outputs specified, defaulting to L/R stereo");
    }

    // Parse nodes
    if (!j.contains("nodes")) {
        result.errors.push_back("Missing 'nodes' section");
        return result;
    }

    for (auto& [node_id, node_json] : j["nodes"].items()) {
        GraphNode node;
        node.id = node_id;

        if (!node_json.contains("type")) {
            result.errors.push_back("Node '" + node_id + "' missing 'type' field");
            continue;
        }
        node.type = node_json["type"].get<std::string>();

        // Parse rate (default: audio)
        if (node_json.contains("rate")) {
            std::string rate_str = node_json["rate"].get<std::string>();
            if (rate_str == "control") {
                node.rate = SignalRate::Control;
            } else if (rate_str == "audio") {
                node.rate = SignalRate::Audio;
            } else {
                result.warnings.push_back("Unknown rate '" + rate_str + "' for node '" +
                                          node_id + "', defaulting to audio");
            }
        }

        // Parse inputs (audio inputs)
        if (node_json.contains("inputs")) {
            for (const auto& inp : node_json["inputs"]) {
                node.inputs.push_back(inp.get<std::string>());
            }
        }

        // Parse outputs
        if (node_json.contains("outputs")) {
            for (const auto& out : node_json["outputs"]) {
                node.outputs.push_back(out.get<std::string>());
            }
        }

        // Parse param_inputs (modulatable parameters)
        if (node_json.contains("param_inputs")) {
            for (const auto& pi : node_json["param_inputs"]) {
                node.param_inputs.push_back(pi.get<std::string>());
            }
        }

        // Parse params
        if (node_json.contains("params")) {
            for (auto& [key, val] : node_json["params"].items()) {
                if (val.is_number_integer()) {
                    node.params[key] = val.get<int>();
                } else if (val.is_number_float()) {
                    node.params[key] = val.get<float>();
                } else if (val.is_boolean()) {
                    node.params[key] = val.get<bool>();
                } else if (val.is_string()) {
                    node.params[key] = val.get<std::string>();
                }
            }
        }

        result.graph.nodes[node_id] = std::move(node);
    }

    // Parse connections
    if (j.contains("connections")) {
        for (const auto& conn_json : j["connections"]) {
            GraphConnection conn;

            if (!conn_json.contains("from") || !conn_json.contains("to")) {
                result.errors.push_back("Connection missing 'from' or 'to' field");
                continue;
            }

            conn.from = parse_port_ref(conn_json["from"].get<std::string>());
            conn.to = parse_port_ref(conn_json["to"].get<std::string>());

            result.graph.connections.push_back(conn);
        }
    }

    // Validate
    auto validation_errors = validate(result.graph);
    for (const auto& err : validation_errors) {
        result.errors.push_back(err);
    }

    result.success = result.errors.empty();
    return result;
}

ParseResult DptParser::parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ParseResult result;
        result.errors.push_back("Cannot open file: " + path);
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str());
}

std::vector<std::string> DptParser::validate(const Graph& graph) {
    std::vector<std::string> errors;

    // Check required nodes exist
    bool has_input = false;
    bool has_output = false;

    for (const auto& [id, node] : graph.nodes) {
        if (node.type == "input") has_input = true;
        if (node.type == "output") has_output = true;
    }

    if (!has_input) {
        errors.push_back("Graph missing 'input' node");
    }
    if (!has_output) {
        errors.push_back("Graph missing 'output' node");
    }

    // Collect all valid port references
    std::set<std::string> valid_outputs;  // Ports that can be read from
    std::set<std::string> valid_inputs;   // Ports that can be written to (audio + param)

    for (const auto& [id, node] : graph.nodes) {
        for (const auto& out : node.outputs) {
            valid_outputs.insert(id + ":" + out);
        }
        for (const auto& inp : node.inputs) {
            valid_inputs.insert(id + ":" + inp);
        }
        // Param inputs are also valid connection destinations
        for (const auto& pi : node.param_inputs) {
            valid_inputs.insert(id + ":" + pi);
        }
    }

    // Validate connections
    for (const auto& conn : graph.connections) {
        std::string from_ref = conn.from.node + ":" + conn.from.port;
        std::string to_ref = conn.to.node + ":" + conn.to.port;

        // Check 'from' node exists
        if (graph.nodes.find(conn.from.node) == graph.nodes.end()) {
            errors.push_back("Connection from unknown node: " + conn.from.node);
            continue;
        }

        // Check 'to' node exists
        if (graph.nodes.find(conn.to.node) == graph.nodes.end()) {
            errors.push_back("Connection to unknown node: " + conn.to.node);
            continue;
        }

        // Check port exists on source (must be an output)
        if (valid_outputs.find(from_ref) == valid_outputs.end()) {
            errors.push_back("Invalid source port: " + from_ref);
        }

        // Check port exists on destination (must be an input)
        if (valid_inputs.find(to_ref) == valid_inputs.end()) {
            errors.push_back("Invalid destination port: " + to_ref);
        }
    }

    // Check for unconnected input ports (warnings, not errors)
    // This would require tracking which inputs are connected

    return errors;
}
