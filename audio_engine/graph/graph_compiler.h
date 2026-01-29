#pragma once

#include "graph_ast.h"
#include "../core/dsp_program.h"
#include <string>
#include <vector>
#include <memory>

// Result of compilation
struct CompileResult {
    bool success = false;
    DSPProgram* program = nullptr;  // Caller takes ownership
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    // Metadata for debugging
    std::vector<std::string> node_order;  // Topological order
    uint32_t num_buffers = 0;
    uint32_t buffer_size = 0;
};

// Compilation context passed to node compilers
struct CompileContext {
    uint32_t sample_rate;
    uint32_t buffer_size;  // samples per buffer (frames * channels)
    uint32_t channels;

    // Buffer allocation
    std::unordered_map<std::string, uint32_t> port_to_buffer;  // "node:port" -> buffer index
    uint32_t next_buffer = 0;

    // State allocation (for effects that need persistent state)
    std::vector<void*> allocated_states;  // To be freed on program destroy
};

// Graph Compiler - Phase 7
// Compiles a normalized Graph into a DSPProgram
class GraphCompiler {
public:
    // Compile a graph to DSPProgram
    // sample_rate: audio sample rate (e.g., 48000)
    // buffer_frames: frames per buffer (e.g., 64)
    // channels: number of channels (e.g., 2 for stereo)
    static CompileResult compile(const Graph& graph,
                                  uint32_t sample_rate,
                                  uint32_t buffer_frames,
                                  uint32_t channels);

    // Free a compiled program and its allocated states
    static void destroy(CompileResult& result);

private:
    // Topological sort of nodes
    static std::vector<std::string> topological_sort(const Graph& graph,
                                                      std::vector<std::string>& errors);

    // Allocate a buffer index for a port
    static uint32_t allocate_buffer(CompileContext& ctx, const std::string& port_ref);

    // Get or allocate buffer for a port
    static uint32_t get_buffer(CompileContext& ctx, const std::string& port_ref);

    // Compile a single node to DSPBlocks
    static std::vector<DSPBlock> compile_node(const GraphNode& node,
                                               const Graph& graph,
                                               CompileContext& ctx,
                                               std::vector<std::string>& errors);
};
