#include "io/alsa_device.h"
#include "engine/audio_engine.h"
#include "graph/dpt_parser.h"
#include "graph/graph_normalizer.h"
#include "graph/graph_compiler.h"
#include "core/dsp_program_builder.h"
#include "core/dsp_ops.h"
#include <cstdio>
#include <csignal>
#include <atomic>
#include <unistd.h>
#include <cstring>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false, std::memory_order_release);
}

void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options] [buffer_size]\n", prog);
    fprintf(stderr, "       %s -f <file.dpt> [buffer_size]\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -f <file>   Load DSP graph from .dpt file\n");
    fprintf(stderr, "  buffer_size Frames per buffer (default: 64)\n");
}

int main(int argc, char* argv[]) {
    fprintf(stderr, "Dendrophone Audio Engine\n");
    fprintf(stderr, "========================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Parse arguments
    const char* dpt_file = nullptr;
    int buffer_frames = 64;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            dpt_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            buffer_frames = atoi(argv[i]);
        }
    }

    AlsaAudioDevice::Config config;
    config.capture_device = "hw:2,0";
    config.playback_device = "hw:2,0";
    config.sample_rate = 48000;
    config.buffer_frames = buffer_frames;
    config.periods = 2;
    config.channels = 2;

    fprintf(stderr, "Buffer size: %d frames (%.2f ms)\n",
            config.buffer_frames,
            (float)config.buffer_frames / config.sample_rate * 1000.0f);

    AlsaAudioDevice device(config);

    if (!device.open()) {
        fprintf(stderr, "Failed to open audio device\n");
        return 1;
    }

    DSPProgram* program = nullptr;
    CompileResult compile_result;
    bool using_compiled_graph = false;

    if (dpt_file) {
        // Load and compile .dpt file
        fprintf(stderr, "\nLoading: %s\n", dpt_file);

        auto parse_result = DptParser::parse_file(dpt_file);
        if (!parse_result.success) {
            fprintf(stderr, "Parse failed:\n");
            for (const auto& e : parse_result.errors) {
                fprintf(stderr, "  %s\n", e.c_str());
            }
            device.close();
            return 1;
        }

        auto norm_result = GraphNormalizer::normalize(parse_result.graph);
        if (!norm_result.success) {
            fprintf(stderr, "Normalize failed:\n");
            for (const auto& e : norm_result.errors) {
                fprintf(stderr, "  %s\n", e.c_str());
            }
            device.close();
            return 1;
        }

        compile_result = GraphCompiler::compile(norm_result.graph,
                                                 config.sample_rate,
                                                 config.buffer_frames,
                                                 config.channels,
                                                 norm_result.control_nodes,
                                                 norm_result.audio_nodes);
        if (!compile_result.success) {
            fprintf(stderr, "Compile failed:\n");
            for (const auto& e : compile_result.errors) {
                fprintf(stderr, "  %s\n", e.c_str());
            }
            device.close();
            return 1;
        }

        program = compile_result.program;
        using_compiled_graph = true;

        fprintf(stderr, "Compiled: %u control blocks, %u audio blocks, %u buffers, %u control buffers\n",
                program->num_control_blocks, program->num_audio_blocks,
                compile_result.num_buffers, compile_result.num_control_buffers);
    } else {
        // Default: passthrough
        fprintf(stderr, "\nNo .dpt file specified, using passthrough\n");

        // buffer_size = frames (not frames*channels)
        DSPProgramBuilder builder(config.buffer_frames, 4, config.channels);
        builder.set_input_buffer(0);
        builder.set_output_buffer(2);

        builder.add_block(copy_op, 0, 0, 2, nullptr);  // L -> L
        builder.add_block(copy_op, 1, 0, 3, nullptr);  // R -> R

        program = builder.build();
        if (!program) {
            fprintf(stderr, "Failed to build passthrough program\n");
            device.close();
            return 1;
        }
    }

    AudioEngine engine(&device);
    engine.set_program(program);

    if (!engine.start()) {
        fprintf(stderr, "Failed to start engine\n");
        if (using_compiled_graph) {
            GraphCompiler::destroy(compile_result);
        } else {
            DSPProgramBuilder::destroy(program);
        }
        device.close();
        return 1;
    }

    fprintf(stderr, "\nRunning. Press Ctrl+C to stop.\n");
    fprintf(stderr, "Latency: %.2f ms (%.2f ms round-trip)\n",
            (float)config.buffer_frames / config.sample_rate * 1000.0f,
            (float)config.buffer_frames / config.sample_rate * 1000.0f * 2.0f);

    while (g_running.load(std::memory_order_acquire)) {
        usleep(100000);
    }

    fprintf(stderr, "\nStopping...\n");

    engine.stop();

    if (using_compiled_graph) {
        GraphCompiler::destroy(compile_result);
    } else {
        DSPProgramBuilder::destroy(program);
    }

    device.close();

    fprintf(stderr, "Done.\n");
    return 0;
}
