#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

// Parameter value types supported in .dpt files
using ParamValue = std::variant<int, float, bool, std::string>;
using ParamMap = std::unordered_map<std::string, ParamValue>;

// Reference to a specific port on a node (e.g., "delay1:out")
struct PortRef {
    std::string node;
    std::string port;

    bool operator==(const PortRef& other) const {
        return node == other.node && port == other.port;
    }
};

// A connection between two ports
struct GraphConnection {
    PortRef from;
    PortRef to;
};

// A node in the graph
struct GraphNode {
    std::string id;
    std::string type;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    ParamMap params;
};

// The complete graph AST parsed from a .dpt file
struct Graph {
    int version = 0;
    std::vector<std::string> audio_inputs;   // e.g., ["L", "R"]
    std::vector<std::string> audio_outputs;  // e.g., ["L", "R"]
    std::unordered_map<std::string, GraphNode> nodes;
    std::vector<GraphConnection> connections;
};

// Helper to get param as specific type with default
template<typename T>
T get_param(const ParamMap& params, const std::string& key, T default_value) {
    auto it = params.find(key);
    if (it == params.end()) return default_value;
    if (auto* val = std::get_if<T>(&it->second)) {
        return *val;
    }
    return default_value;
}

// Specialization for float that also accepts int
template<>
inline float get_param<float>(const ParamMap& params, const std::string& key, float default_value) {
    auto it = params.find(key);
    if (it == params.end()) return default_value;
    if (auto* val = std::get_if<float>(&it->second)) {
        return *val;
    }
    if (auto* val = std::get_if<int>(&it->second)) {
        return static_cast<float>(*val);
    }
    return default_value;
}
