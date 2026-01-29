// Test program for full compilation pipeline
// .dpt → Parse → Normalize → Compile → DSPProgram

#include "../graph/dpt_parser.h"
#include "../graph/graph_normalizer.h"
#include "../graph/graph_compiler.h"
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file.dpt>\n", argv[0]);
        return 1;
    }

    printf("=== Phase 5: Parsing ===\n");
    auto parse_result = DptParser::parse_file(argv[1]);

    if (!parse_result.warnings.empty()) {
        for (const auto& w : parse_result.warnings) {
            printf("[WARN] %s\n", w.c_str());
        }
    }

    if (!parse_result.success) {
        printf("Parse FAILED:\n");
        for (const auto& e : parse_result.errors) {
            printf("  [ERROR] %s\n", e.c_str());
        }
        return 1;
    }
    printf("Parse OK: %zu nodes, %zu connections\n",
           parse_result.graph.nodes.size(),
           parse_result.graph.connections.size());

    printf("\n=== Phase 6: Normalizing ===\n");
    auto norm_result = GraphNormalizer::normalize(parse_result.graph);

    if (!norm_result.warnings.empty()) {
        for (const auto& w : norm_result.warnings) {
            printf("[WARN] %s\n", w.c_str());
        }
    }

    if (!norm_result.success) {
        printf("Normalize FAILED:\n");
        for (const auto& e : norm_result.errors) {
            printf("  [ERROR] %s\n", e.c_str());
        }
        return 1;
    }
    printf("Normalize OK: %zu nodes, %zu connections\n",
           norm_result.graph.nodes.size(),
           norm_result.graph.connections.size());

    // Show any auto-generated nodes
    for (const auto& [id, node] : norm_result.graph.nodes) {
        if (id.find("_mix_") == 0) {
            printf("  Auto-generated: %s (type: %s, %zu inputs)\n",
                   id.c_str(), node.type.c_str(), node.inputs.size());
        }
    }

    printf("\n=== Phase 7: Compiling ===\n");
    uint32_t sample_rate = 48000;
    uint32_t buffer_frames = 64;
    uint32_t channels = 2;

    auto compile_result = GraphCompiler::compile(norm_result.graph,
                                                  sample_rate,
                                                  buffer_frames,
                                                  channels);

    if (!compile_result.warnings.empty()) {
        for (const auto& w : compile_result.warnings) {
            printf("[WARN] %s\n", w.c_str());
        }
    }

    if (!compile_result.success) {
        printf("Compile FAILED:\n");
        for (const auto& e : compile_result.errors) {
            printf("  [ERROR] %s\n", e.c_str());
        }
        return 1;
    }

    printf("Compile OK!\n");
    printf("  Topological order: ");
    for (const auto& node : compile_result.node_order) {
        printf("%s ", node.c_str());
    }
    printf("\n");
    printf("  Buffers: %u (size: %u samples each)\n",
           compile_result.num_buffers, compile_result.buffer_size);
    printf("  DSP blocks: %u\n", compile_result.program->num_blocks);

    // Show blocks
    printf("\nDSP Program:\n");
    for (uint32_t i = 0; i < compile_result.program->num_blocks; i++) {
        auto& block = compile_result.program->blocks[i];
        printf("  [%u] buf[%u] + buf[%u] -> buf[%u]",
               i, block.in_a, block.in_b, block.out);
        if (block.state) printf(" (has state)");
        printf("\n");
    }

    // Test the program with dummy data
    printf("\n=== Testing DSP Program ===\n");
    uint32_t test_frames = buffer_frames;
    uint32_t test_samples = test_frames * channels;  // Interleaved
    float* test_input = new float[test_samples];
    float* test_output = new float[test_samples];

    // Fill with test signal (interleaved stereo)
    for (uint32_t i = 0; i < test_frames; i++) {
        test_input[i * 2 + 0] = (i % 100) / 100.0f;  // L: sawtooth
        test_input[i * 2 + 1] = (i % 50) / 50.0f;    // R: faster sawtooth
    }

    // Process (pass frames, not samples)
    compile_result.program->process(test_input, test_output, test_frames);

    // Show some output (interleaved: L0, R0, L1, R1, ...)
    printf("Input  L[0..3]: %.3f %.3f %.3f %.3f\n",
           test_input[0], test_input[2], test_input[4], test_input[6]);
    printf("Input  R[0..3]: %.3f %.3f %.3f %.3f\n",
           test_input[1], test_input[3], test_input[5], test_input[7]);
    printf("Output L[0..3]: %.3f %.3f %.3f %.3f\n",
           test_output[0], test_output[2], test_output[4], test_output[6]);
    printf("Output R[0..3]: %.3f %.3f %.3f %.3f\n",
           test_output[1], test_output[3], test_output[5], test_output[7]);

    delete[] test_input;
    delete[] test_output;

    // Cleanup
    GraphCompiler::destroy(compile_result);

    printf("\n=== SUCCESS ===\n");
    return 0;
}
