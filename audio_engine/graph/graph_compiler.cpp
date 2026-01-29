#include "graph_compiler.h"
#include "../core/dsp_program_builder.h"
#include "../core/dsp_ops.h"
#include "../effects/delay.h"
#include "../effects/lfo.h"
#include <algorithm>
#include <queue>
#include <map>

// Topological sort using Kahn's algorithm for a subset of nodes
std::vector<std::string> GraphCompiler::topological_sort(const Graph& graph,
                                                          const std::set<std::string>& subset,
                                                          std::vector<std::string>& errors) {
    if (subset.empty()) {
        return {};
    }

    // Build adjacency list and in-degree count for subset only
    std::map<std::string, std::set<std::string>> dependencies;
    std::map<std::string, int> in_degree;

    // Initialize subset nodes
    for (const auto& id : subset) {
        dependencies[id] = {};
        in_degree[id] = 0;
    }

    // Build dependency graph from connections (only within subset)
    for (const auto& conn : graph.connections) {
        if (subset.count(conn.to.node) && subset.count(conn.from.node)) {
            if (dependencies[conn.to.node].insert(conn.from.node).second) {
                in_degree[conn.to.node]++;
            }
        }
    }

    // Kahn's algorithm
    std::queue<std::string> ready;
    for (const auto& [id, degree] : in_degree) {
        if (degree == 0) {
            ready.push(id);
        }
    }

    std::vector<std::string> sorted;
    while (!ready.empty()) {
        std::string node = ready.front();
        ready.pop();
        sorted.push_back(node);

        for (auto& [id, deps] : dependencies) {
            if (deps.erase(node)) {
                in_degree[id]--;
                if (in_degree[id] == 0) {
                    ready.push(id);
                }
            }
        }
    }

    if (sorted.size() != subset.size()) {
        errors.push_back("Graph contains a cycle - cannot compile");
    }

    return sorted;
}

uint32_t GraphCompiler::allocate_buffer(CompileContext& ctx, const std::string& port_ref) {
    uint32_t idx = ctx.next_buffer++;
    ctx.port_to_buffer[port_ref] = idx;
    return idx;
}

uint32_t GraphCompiler::allocate_control_buffer(CompileContext& ctx, const std::string& port_ref) {
    uint32_t idx = ctx.next_control_buffer++;
    ctx.port_to_control_buffer[port_ref] = idx;
    return idx;
}

uint32_t GraphCompiler::get_buffer(CompileContext& ctx, const std::string& port_ref) {
    auto it = ctx.port_to_buffer.find(port_ref);
    if (it != ctx.port_to_buffer.end()) {
        return it->second;
    }
    return allocate_buffer(ctx, port_ref);
}

uint32_t GraphCompiler::get_control_buffer(CompileContext& ctx, const std::string& port_ref) {
    auto it = ctx.port_to_control_buffer.find(port_ref);
    if (it != ctx.port_to_control_buffer.end()) {
        return it->second;
    }
    return allocate_control_buffer(ctx, port_ref);
}

std::vector<DSPBlock> GraphCompiler::compile_control_node(const GraphNode& node,
                                                           const Graph& graph,
                                                           CompileContext& ctx,
                                                           std::vector<std::string>& errors) {
    std::vector<DSPBlock> blocks;

    if (node.type == "lfo") {
        // Parse LFO parameters
        std::string wave_str = get_param<std::string>(node.params, "wave", "sine");
        LFOWave wave = lfo_wave_from_string(wave_str);
        float freq_hz = get_param<float>(node.params, "freq_hz", 1.0f);
        float depth = get_param<float>(node.params, "depth", 1.0f);

        // Create LFO state
        LFOState* state = lfo_state_create(wave, freq_hz, depth, ctx.sample_rate);
        if (!state) {
            errors.push_back("Failed to allocate LFO state for " + node.id);
            return blocks;
        }
        ctx.allocated_states.push_back(state);

        // Allocate control output buffer
        std::string out_ref = node.id + ":value";
        uint32_t out_buf = allocate_control_buffer(ctx, out_ref);

        DSPBlock block;
        block.fn = lfo_op;
        block.in_a = 0;
        block.in_b = 0;
        block.out = out_buf;
        block.state = state;
        blocks.push_back(block);
    }
    else {
        errors.push_back("Unknown control node type: " + node.type);
    }

    return blocks;
}

std::vector<DSPBlock> GraphCompiler::compile_audio_node(const GraphNode& node,
                                                         const Graph& graph,
                                                         CompileContext& ctx,
                                                         std::vector<std::string>& errors) {
    std::vector<DSPBlock> blocks;

    if (node.type == "input") {
        // Input node: maps to ADC buffers (already set up)
        // No DSP blocks needed
    }
    else if (node.type == "output") {
        // Output node: copy from sources to output buffers
        for (size_t i = 0; i < node.inputs.size(); i++) {
            std::string in_port = node.id + ":" + node.inputs[i];

            for (const auto& conn : graph.connections) {
                if (conn.to.node == node.id && conn.to.port == node.inputs[i]) {
                    std::string src_ref = conn.from.node + ":" + conn.from.port;
                    uint32_t src_buf = get_buffer(ctx, src_ref);
                    uint32_t out_buf = ctx.channels + i;

                    DSPBlock block;
                    block.fn = copy_op;
                    block.in_a = src_buf;
                    block.in_b = 0;
                    block.out = out_buf;
                    block.state = nullptr;
                    blocks.push_back(block);
                    break;
                }
            }
        }
    }
    else if (node.type == "delay") {
        float time_ms = get_param<float>(node.params, "time_ms", 250.0f);
        float feedback = get_param<float>(node.params, "feedback", 0.0f);
        float mix = get_param<float>(node.params, "mix", 1.0f);

        uint32_t delay_samples = static_cast<uint32_t>(time_ms * ctx.sample_rate / 1000.0f);
        uint32_t max_delay = ctx.sample_rate * 2;

        // Create delay state with modulatable params
        DelayState* state = delay_state_create(max_delay, delay_samples, feedback, mix,
                                                static_cast<float>(ctx.sample_rate));
        if (!state) {
            errors.push_back("Failed to allocate delay state for " + node.id);
            return blocks;
        }
        ctx.allocated_states.push_back(state);

        // Register modulatable parameters
        ctx.params[node.id + ":time_ms"] = &state->time_ms;
        ctx.params[node.id + ":feedback"] = &state->feedback;
        ctx.params[node.id + ":mix"] = &state->mix;

        // Find input buffer
        uint32_t in_buf = 0;
        for (const auto& conn : graph.connections) {
            if (conn.to.node == node.id && conn.to.port == "in") {
                std::string src_ref = conn.from.node + ":" + conn.from.port;
                in_buf = get_buffer(ctx, src_ref);
                break;
            }
        }

        std::string out_ref = node.id + ":out";
        uint32_t out_buf = allocate_buffer(ctx, out_ref);

        DSPBlock block;
        block.fn = delay_op;
        block.in_a = in_buf;
        block.in_b = 0;
        block.out = out_buf;
        block.state = state;
        blocks.push_back(block);
    }
    else if (node.type == "gain") {
        float gain_val = get_param<float>(node.params, "gain", 1.0f);

        GainState* state = new GainState{gain_val};
        ctx.allocated_states.push_back(state);

        uint32_t in_buf = 0;
        for (const auto& conn : graph.connections) {
            if (conn.to.node == node.id && conn.to.port == "in") {
                std::string src_ref = conn.from.node + ":" + conn.from.port;
                in_buf = get_buffer(ctx, src_ref);
                break;
            }
        }

        std::string out_ref = node.id + ":out";
        uint32_t out_buf = allocate_buffer(ctx, out_ref);

        DSPBlock block;
        block.fn = gain_op;
        block.in_a = in_buf;
        block.in_b = 0;
        block.out = out_buf;
        block.state = state;
        blocks.push_back(block);
    }
    else if (node.type == "mix") {
        std::string out_ref = node.id + ":out";
        uint32_t out_buf = allocate_buffer(ctx, out_ref);

        bool first = true;
        for (const auto& in_port : node.inputs) {
            for (const auto& conn : graph.connections) {
                if (conn.to.node == node.id && conn.to.port == in_port) {
                    std::string src_ref = conn.from.node + ":" + conn.from.port;
                    uint32_t src_buf = get_buffer(ctx, src_ref);

                    if (first) {
                        DSPBlock block;
                        block.fn = copy_op;
                        block.in_a = src_buf;
                        block.in_b = 0;
                        block.out = out_buf;
                        block.state = nullptr;
                        blocks.push_back(block);
                        first = false;
                    } else {
                        DSPBlock block;
                        block.fn = mix_op;
                        block.in_a = out_buf;
                        block.in_b = src_buf;
                        block.out = out_buf;
                        block.state = nullptr;
                        blocks.push_back(block);
                    }
                    break;
                }
            }
        }
    }
    else if (node.type == "copy" || node.type == "passthrough") {
        uint32_t in_buf = 0;
        for (const auto& conn : graph.connections) {
            if (conn.to.node == node.id) {
                std::string src_ref = conn.from.node + ":" + conn.from.port;
                in_buf = get_buffer(ctx, src_ref);
                break;
            }
        }

        std::string out_ref = node.id + ":out";
        uint32_t out_buf = allocate_buffer(ctx, out_ref);

        DSPBlock block;
        block.fn = copy_op;
        block.in_a = in_buf;
        block.in_b = 0;
        block.out = out_buf;
        block.state = nullptr;
        blocks.push_back(block);
    }
    else {
        errors.push_back("Unknown audio node type: " + node.type);
    }

    return blocks;
}

void GraphCompiler::build_mod_routes(const Graph& graph,
                                      CompileContext& ctx,
                                      std::vector<std::string>& errors) {
    // Find all connections from control nodes to param_inputs
    for (const auto& conn : graph.connections) {
        if (ctx.control_nodes.count(conn.from.node) == 0) {
            continue;  // Not from a control node
        }

        // Check if destination is a param_input
        auto to_node_it = graph.nodes.find(conn.to.node);
        if (to_node_it == graph.nodes.end()) continue;

        const auto& to_node = to_node_it->second;
        bool is_param = false;
        for (const auto& pi : to_node.param_inputs) {
            if (pi == conn.to.port) {
                is_param = true;
                break;
            }
        }

        if (!is_param) continue;

        // Get control buffer for source
        std::string src_ref = conn.from.node + ":" + conn.from.port;
        auto ctrl_it = ctx.port_to_control_buffer.find(src_ref);
        if (ctrl_it == ctx.port_to_control_buffer.end()) {
            errors.push_back("Control output not found: " + src_ref);
            continue;
        }

        // Get param pointer for destination
        std::string param_ref = conn.to.node + ":" + conn.to.port;
        auto param_it = ctx.params.find(param_ref);
        if (param_it == ctx.params.end()) {
            // Param not registered - this is expected for nodes that
            // don't yet support modulation
            // For now, just warn
            // errors.push_back("Parameter not modulatable: " + param_ref);
            continue;
        }

        ModRoute route;
        route.control_buffer = ctrl_it->second;
        route.target = param_it->second;
        route.scale = 1.0f;  // Could be configurable
        ctx.mod_routes.push_back(route);
    }
}

CompileResult GraphCompiler::compile(const Graph& graph,
                                      uint32_t sample_rate,
                                      uint32_t buffer_frames,
                                      uint32_t channels,
                                      const std::set<std::string>& control_nodes,
                                      const std::set<std::string>& audio_nodes) {
    CompileResult result;

    // Initialize context
    CompileContext ctx;
    ctx.sample_rate = sample_rate;
    ctx.buffer_size = buffer_frames;
    ctx.channels = channels;
    ctx.control_nodes = control_nodes;
    ctx.audio_nodes = audio_nodes;

    // If not provided, classify nodes
    if (ctx.control_nodes.empty() && ctx.audio_nodes.empty()) {
        for (const auto& [id, node] : graph.nodes) {
            if (node.rate == SignalRate::Control) {
                ctx.control_nodes.insert(id);
            } else {
                ctx.audio_nodes.insert(id);
            }
        }
    }

    // Reserve buffers for input channels (0 to channels-1)
    for (uint32_t i = 0; i < channels; i++) {
        ctx.next_buffer++;
    }

    // Map input node ports to input buffers
    for (const auto& [id, node] : graph.nodes) {
        if (node.type == "input") {
            for (size_t i = 0; i < node.outputs.size() && i < channels; i++) {
                std::string port_ref = id + ":" + node.outputs[i];
                ctx.port_to_buffer[port_ref] = i;
            }
            break;
        }
    }

    // Reserve buffers for output channels
    uint32_t output_buffer_start = ctx.next_buffer;
    for (uint32_t i = 0; i < channels; i++) {
        ctx.next_buffer++;
    }

    // Sort and compile control nodes first
    result.control_order = topological_sort(graph, ctx.control_nodes, result.errors);
    if (!result.errors.empty()) {
        return result;
    }

    std::vector<DSPBlock> control_blocks;
    for (const std::string& node_id : result.control_order) {
        const auto& node = graph.nodes.at(node_id);
        auto blocks = compile_control_node(node, graph, ctx, result.errors);
        for (auto& b : blocks) {
            control_blocks.push_back(b);
        }
    }

    // Sort and compile audio nodes
    result.audio_order = topological_sort(graph, ctx.audio_nodes, result.errors);
    if (!result.errors.empty()) {
        for (void* state : ctx.allocated_states) {
            free(state);
        }
        return result;
    }

    std::vector<DSPBlock> audio_blocks;
    for (const std::string& node_id : result.audio_order) {
        const auto& node = graph.nodes.at(node_id);
        auto blocks = compile_audio_node(node, graph, ctx, result.errors);
        for (auto& b : blocks) {
            audio_blocks.push_back(b);
        }
    }

    if (!result.errors.empty()) {
        for (void* state : ctx.allocated_states) {
            free(state);
        }
        return result;
    }

    // Build modulation routes
    build_mod_routes(graph, ctx, result.errors);

    // Build the DSPProgram
    result.num_buffers = ctx.next_buffer;
    result.num_control_buffers = ctx.next_control_buffer;
    result.buffer_size = ctx.buffer_size;

    DSPProgramBuilder builder(ctx.buffer_size, ctx.next_buffer, channels);
    builder.set_input_buffer(0);
    builder.set_output_buffer(output_buffer_start);
    builder.set_num_control_buffers(ctx.next_control_buffer);

    // Add control blocks
    for (const auto& block : control_blocks) {
        builder.add_control_block(block.fn, block.in_a, block.in_b, block.out, block.state);
    }

    // Add audio blocks
    for (const auto& block : audio_blocks) {
        builder.add_audio_block(block.fn, block.in_a, block.in_b, block.out, block.state);
    }

    // Add mod routes
    for (const auto& route : ctx.mod_routes) {
        builder.add_mod_route(route.control_buffer, route.target, route.scale);
    }

    result.program = builder.build();
    if (!result.program) {
        result.errors.push_back("Failed to build DSPProgram");
        for (void* state : ctx.allocated_states) {
            free(state);
        }
        return result;
    }

    result.success = true;
    return result;
}

void GraphCompiler::destroy(CompileResult& result) {
    if (result.program) {
        // Free control block states
        for (uint32_t i = 0; i < result.program->num_control_blocks; i++) {
            if (result.program->control_blocks[i].state) {
                free(result.program->control_blocks[i].state);
            }
        }
        // Free audio block states
        for (uint32_t i = 0; i < result.program->num_audio_blocks; i++) {
            if (result.program->audio_blocks[i].state) {
                free(result.program->audio_blocks[i].state);
            }
        }
        DSPProgramBuilder::destroy(result.program);
        result.program = nullptr;
    }
    result.success = false;
}
