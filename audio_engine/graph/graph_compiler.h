#pragma once

#include "graph_ast.h"
#include "graph_normalizer.h"
#include "../core/dsp_program.h"
#include "../core/dsp_param.h"
#include <string>
#include <vector>
#include <memory>
#include <set>

// Result of compilation
struct CompileResult {
    bool success = false;
    DSPProgram* program = nullptr;  // Caller takes ownership
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    // Metadata for debugging
    std::vector<std::string> control_order;  // Control node order
    std::vector<std::string> audio_order;    // Audio node order
    uint32_t num_buffers = 0;
    uint32_t num_control_buffers = 0;
    uint32_t buffer_size = 0;
};

// Compilation context passed to node compilers
struct CompileContext {
    uint32_t sample_rate;
    uint32_t buffer_size;  // samples per buffer (frames)
    uint32_t channels;

    // Audio buffer allocation
    std::unordered_map<std::string, uint32_t> port_to_buffer;  // "node:port" -> buffer index
    uint32_t next_buffer = 0;

    // Control buffer allocation
    std::unordered_map<std::string, uint32_t> port_to_control_buffer;
    uint32_t next_control_buffer = 0;

    // Parameter tracking: "node:param" -> Param*
    std::unordered_map<std::string, Param*> params;

    // State allocation (for effects that need persistent state)
    std::vector<void*> allocated_states;  // To be freed on program destroy

    // Modulation routes to build
    std::vector<ModRoute> mod_routes;

    // Node classification
    std::set<std::string> control_nodes;
    std::set<std::string> audio_nodes;
};

// Graph Compiler - Phase 7
// Compiles a normalized Graph into a DSPProgram
class GraphCompiler {
public:
    // Compile a graph to DSPProgram
    // Uses NormalizeResult to get node classification
    static CompileResult compile(const Graph& graph,
                                  uint32_t sample_rate,
                                  uint32_t buffer_frames,
                                  uint32_t channels,
                                  const std::set<std::string>& control_nodes = {},
                                  const std::set<std::string>& audio_nodes = {});

    // Free a compiled program and its allocated states
    static void destroy(CompileResult& result);

private:
    // Topological sort of a subset of nodes
    static std::vector<std::string> topological_sort(const Graph& graph,
                                                      const std::set<std::string>& subset,
                                                      std::vector<std::string>& errors);

    // Allocate a buffer index for a port
    static uint32_t allocate_buffer(CompileContext& ctx, const std::string& port_ref);
    static uint32_t allocate_control_buffer(CompileContext& ctx, const std::string& port_ref);

    // Get or allocate buffer for a port
    static uint32_t get_buffer(CompileContext& ctx, const std::string& port_ref);
    static uint32_t get_control_buffer(CompileContext& ctx, const std::string& port_ref);

    // Compile control node to DSPBlock
    static std::vector<DSPBlock> compile_control_node(const GraphNode& node,
                                                       const Graph& graph,
                                                       CompileContext& ctx,
                                                       std::vector<std::string>& errors);

    // Compile audio node to DSPBlocks
    static std::vector<DSPBlock> compile_audio_node(const GraphNode& node,
                                                     const Graph& graph,
                                                     CompileContext& ctx,
                                                     std::vector<std::string>& errors);

    // Build modulation routes from connections
    static void build_mod_routes(const Graph& graph,
                                  CompileContext& ctx,
                                  std::vector<std::string>& errors);
};
