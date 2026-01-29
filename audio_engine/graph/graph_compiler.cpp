#include "graph_compiler.h"
#include "../core/dsp_program_builder.h"
#include "../core/dsp_ops.h"
#include "../effects/delay.h"
#include <algorithm>
#include <queue>
#include <set>
#include <map>

// Topological sort using Kahn's algorithm
std::vector<std::string> GraphCompiler::topological_sort(const Graph& graph,
                                                          std::vector<std::string>& errors) {
    // Build adjacency list and in-degree count
    std::map<std::string, std::set<std::string>> dependencies;  // node -> nodes it depends on
    std::map<std::string, int> in_degree;

    // Initialize all nodes
    for (const auto& [id, node] : graph.nodes) {
        dependencies[id] = {};
        in_degree[id] = 0;
    }

    // Build dependency graph from connections
    for (const auto& conn : graph.connections) {
        // conn.to depends on conn.from
        if (dependencies[conn.to.node].insert(conn.from.node).second) {
            in_degree[conn.to.node]++;
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

        // Reduce in-degree of nodes that depend on this one
        for (auto& [id, deps] : dependencies) {
            if (deps.erase(node)) {
                in_degree[id]--;
                if (in_degree[id] == 0) {
                    ready.push(id);
                }
            }
        }
    }

    // Check for cycles
    if (sorted.size() != graph.nodes.size()) {
        errors.push_back("Graph contains a cycle - cannot compile");
    }

    return sorted;
}

uint32_t GraphCompiler::allocate_buffer(CompileContext& ctx, const std::string& port_ref) {
    uint32_t idx = ctx.next_buffer++;
    ctx.port_to_buffer[port_ref] = idx;
    return idx;
}

uint32_t GraphCompiler::get_buffer(CompileContext& ctx, const std::string& port_ref) {
    auto it = ctx.port_to_buffer.find(port_ref);
    if (it != ctx.port_to_buffer.end()) {
        return it->second;
    }
    return allocate_buffer(ctx, port_ref);
}

std::vector<DSPBlock> GraphCompiler::compile_node(const GraphNode& node,
                                                   const Graph& graph,
                                                   CompileContext& ctx,
                                                   std::vector<std::string>& errors) {
    std::vector<DSPBlock> blocks;

    if (node.type == "input") {
        // Input node: audio comes from ADC
        // The input buffer is special - it's buffer 0 and 1 (L/R interleaved or separate)
        // For simplicity, we use a single interleaved input buffer
        // Output ports map to portions of the input buffer
        // Actually, we just mark where input data should go
        for (const auto& out_port : node.outputs) {
            std::string port_ref = node.id + ":" + out_port;
            // Input buffers are pre-allocated at indices 0, 1, etc.
            // We don't emit blocks for input - the engine handles it
        }
        // No DSP blocks for input node
    }
    else if (node.type == "output") {
        // Output node: audio goes to DAC
        // Find what's connected to each input and copy to output buffers
        for (size_t i = 0; i < node.inputs.size(); i++) {
            std::string in_port = node.id + ":" + node.inputs[i];

            // Find the source connection
            for (const auto& conn : graph.connections) {
                if (conn.to.node == node.id && conn.to.port == node.inputs[i]) {
                    std::string src_ref = conn.from.node + ":" + conn.from.port;
                    uint32_t src_buf = get_buffer(ctx, src_ref);

                    // Output buffer index
                    // Convention: output buffers follow input buffers
                    uint32_t out_buf = ctx.channels + i;  // After input buffers

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
        // Delay effect
        float time_ms = get_param<float>(node.params, "time_ms", 250.0f);
        float feedback = get_param<float>(node.params, "feedback", 0.0f);
        float mix = get_param<float>(node.params, "mix", 1.0f);

        // Calculate delay in samples
        uint32_t delay_samples = static_cast<uint32_t>(time_ms * ctx.sample_rate / 1000.0f);

        // Max delay: 2 seconds
        uint32_t max_delay = ctx.sample_rate * 2;

        // Create delay state
        DelayState* state = delay_state_create(max_delay, delay_samples, feedback, mix);
        if (!state) {
            errors.push_back("Failed to allocate delay state for " + node.id);
            return blocks;
        }
        ctx.allocated_states.push_back(state);

        // Find input buffer
        uint32_t in_buf = 0;
        for (const auto& conn : graph.connections) {
            if (conn.to.node == node.id && conn.to.port == "in") {
                std::string src_ref = conn.from.node + ":" + conn.from.port;
                in_buf = get_buffer(ctx, src_ref);
                break;
            }
        }

        // Allocate output buffer
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

        // Create gain state
        GainState* state = new GainState{gain_val};
        ctx.allocated_states.push_back(state);

        // Find input buffer
        uint32_t in_buf = 0;
        for (const auto& conn : graph.connections) {
            if (conn.to.node == node.id && conn.to.port == "in") {
                std::string src_ref = conn.from.node + ":" + conn.from.port;
                in_buf = get_buffer(ctx, src_ref);
                break;
            }
        }

        // Allocate output buffer
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
        // Mix node (auto-generated by normalizer for fan-ins)
        // Sum all inputs to output

        std::string out_ref = node.id + ":out";
        uint32_t out_buf = allocate_buffer(ctx, out_ref);

        bool first = true;
        for (const auto& in_port : node.inputs) {
            // Find source for this input
            for (const auto& conn : graph.connections) {
                if (conn.to.node == node.id && conn.to.port == in_port) {
                    std::string src_ref = conn.from.node + ":" + conn.from.port;
                    uint32_t src_buf = get_buffer(ctx, src_ref);

                    if (first) {
                        // First input: copy to output
                        DSPBlock block;
                        block.fn = copy_op;
                        block.in_a = src_buf;
                        block.in_b = 0;
                        block.out = out_buf;
                        block.state = nullptr;
                        blocks.push_back(block);
                        first = false;
                    } else {
                        // Subsequent inputs: add to output
                        DSPBlock block;
                        block.fn = mix_op;
                        block.in_a = out_buf;  // Current accumulated value
                        block.in_b = src_buf;  // New value to add
                        block.out = out_buf;   // Result back to output
                        block.state = nullptr;
                        blocks.push_back(block);
                    }
                    break;
                }
            }
        }
    }
    else if (node.type == "copy" || node.type == "passthrough") {
        // Find input buffer
        uint32_t in_buf = 0;
        for (const auto& conn : graph.connections) {
            if (conn.to.node == node.id) {
                std::string src_ref = conn.from.node + ":" + conn.from.port;
                in_buf = get_buffer(ctx, src_ref);
                break;
            }
        }

        // Allocate output buffer
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
        errors.push_back("Unknown node type: " + node.type);
    }

    return blocks;
}

CompileResult GraphCompiler::compile(const Graph& graph,
                                      uint32_t sample_rate,
                                      uint32_t buffer_frames,
                                      uint32_t channels) {
    CompileResult result;

    // Initialize context
    CompileContext ctx;
    ctx.sample_rate = sample_rate;
    ctx.buffer_size = buffer_frames;  // Samples per buffer = frames (mono buffer)
    ctx.channels = channels;

    // Reserve buffers for input channels
    // Buffer 0..channels-1 are input (from ADC)
    for (uint32_t i = 0; i < channels; i++) {
        ctx.next_buffer++;
    }

    // Find input node and map its output ports to input buffers
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
    // These will be filled by the output node compilation
    uint32_t output_buffer_start = ctx.next_buffer;
    for (uint32_t i = 0; i < channels; i++) {
        ctx.next_buffer++;
    }

    // Topological sort
    result.node_order = topological_sort(graph, result.errors);
    if (!result.errors.empty()) {
        return result;
    }

    // Compile each node in order
    std::vector<DSPBlock> all_blocks;
    for (const std::string& node_id : result.node_order) {
        const auto& node = graph.nodes.at(node_id);
        auto node_blocks = compile_node(node, graph, ctx, result.errors);
        for (auto& block : node_blocks) {
            all_blocks.push_back(block);
        }
    }

    if (!result.errors.empty()) {
        // Clean up allocated states
        for (void* state : ctx.allocated_states) {
            free(state);
        }
        return result;
    }

    // Build the DSPProgram
    result.num_buffers = ctx.next_buffer;
    result.buffer_size = ctx.buffer_size;

    DSPProgramBuilder builder(ctx.buffer_size, ctx.next_buffer, channels);
    builder.set_input_buffer(0);  // First input channel
    builder.set_output_buffer(output_buffer_start);  // First output channel

    for (const auto& block : all_blocks) {
        builder.add_block(block.fn, block.in_a, block.in_b, block.out, block.state);
    }

    result.program = builder.build();
    if (!result.program) {
        result.errors.push_back("Failed to build DSPProgram");
        for (void* state : ctx.allocated_states) {
            free(state);
        }
        return result;
    }

    // Transfer state ownership - they're now owned by the program
    // (We track them in result for cleanup)

    result.success = true;
    return result;
}

void GraphCompiler::destroy(CompileResult& result) {
    if (result.program) {
        // Free allocated states
        for (uint32_t i = 0; i < result.program->num_blocks; i++) {
            if (result.program->blocks[i].state) {
                // Check if it's a delay state (has a buffer to free)
                // For now, we just free the state pointer
                // In a real system, we'd need type information
                free(result.program->blocks[i].state);
            }
        }
        DSPProgramBuilder::destroy(result.program);
        result.program = nullptr;
    }
    result.success = false;
}
