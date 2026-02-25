#ifdef __APPLE__
#include "io/portaudio_device.h"
#include <portaudio.h>
#else
#include "io/alsa_device.h"
#endif

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
#include <cstdlib>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false, std::memory_order_release);
}

void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options] [buffer_size]\n", prog);
    fprintf(stderr, "       %s -f <file.dpt> [buffer_size]\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -f <file>          Load DSP graph from .dpt file\n");
    fprintf(stderr, "  buffer_size        Frames per buffer (default: 64)\n");
#ifdef __APPLE__
    fprintf(stderr, "  --list-devices     Print available audio devices and exit\n");
    fprintf(stderr, "  --input-device N   Use device index N for input\n");
    fprintf(stderr, "  --output-device N  Use device index N for output\n");
    fprintf(stderr, "  --input-name STR   Use first input device whose name contains STR\n");
    fprintf(stderr, "  --output-name STR  Use first output device whose name contains STR\n");
#endif
}

#ifdef __APPLE__
// Find the first device index whose name contains `substr` and has channels
// in the requested direction (0 = input, 1 = output).
static int find_device_by_name(const char* substr, int direction) {
    Pa_Initialize();
    int count = Pa_GetDeviceCount();
    int result = -1;
    for (int i = 0; i < count; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        bool has_channels = (direction == 0)
                            ? info->maxInputChannels  > 0
                            : info->maxOutputChannels > 0;
        if (has_channels && strstr(info->name, substr) != nullptr) {
            result = i;
            break;
        }
    }
    Pa_Terminate();
    return result;
}
#endif

int main(int argc, char* argv[]) {
    fprintf(stderr, "Dendrophone Audio Engine\n");
    fprintf(stderr, "========================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Parse arguments
    const char* dpt_file  = nullptr;
    int buffer_frames = 64;
    int sample_rate   = 48000;

#ifdef __APPLE__
    int  input_device_idx  = -1;
    int  output_device_idx = -1;
    const char* input_name  = nullptr;
    const char* output_name = nullptr;
    bool list_devices_flag  = false;
#endif

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            dpt_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
#ifdef __APPLE__
        } else if (strcmp(argv[i], "--list-devices") == 0) {
            list_devices_flag = true;
        } else if (strcmp(argv[i], "--input-device") == 0 && i + 1 < argc) {
            input_device_idx = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--output-device") == 0 && i + 1 < argc) {
            output_device_idx = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--input-name") == 0 && i + 1 < argc) {
            input_name = argv[++i];
        } else if (strcmp(argv[i], "--output-name") == 0 && i + 1 < argc) {
            output_name = argv[++i];
#endif
        } else if (argv[i][0] != '-') {
            buffer_frames = atoi(argv[i]);
        }
    }

#ifdef __APPLE__
    if (list_devices_flag) {
        PortAudioDevice::list_devices();
        return 0;
    }

    // Resolve name-based device selection
    if (input_name && input_device_idx < 0) {
        input_device_idx = find_device_by_name(input_name, 0);
        if (input_device_idx < 0) {
            fprintf(stderr, "No input device found matching '%s'\n", input_name);
            PortAudioDevice::list_devices();
            return 1;
        }
        fprintf(stderr, "Input:  matched '%s' -> index %d\n", input_name, input_device_idx);
    }
    if (output_name && output_device_idx < 0) {
        output_device_idx = find_device_by_name(output_name, 1);
        if (output_device_idx < 0) {
            fprintf(stderr, "No output device found matching '%s'\n", output_name);
            PortAudioDevice::list_devices();
            return 1;
        }
        fprintf(stderr, "Output: matched '%s' -> index %d\n", output_name, output_device_idx);
    }

    PortAudioDevice::Config config;
    config.input_device_index  = input_device_idx;
    config.output_device_index = output_device_idx;
    config.sample_rate   = sample_rate;
    config.buffer_frames = buffer_frames;
    config.channels      = 2;

    PortAudioDevice device(config);
#else
    AlsaAudioDevice::Config config;
    config.capture_device  = "hw:0,0";
    config.playback_device = "hw:0,0";
    config.sample_rate   = sample_rate;
    config.buffer_frames = buffer_frames;
    config.periods       = 2;
    config.channels      = 2;

    AlsaAudioDevice device(config);
#endif

    fprintf(stderr, "Buffer size: %d frames (%.2f ms)\n",
            config.buffer_frames,
            (float)config.buffer_frames / config.sample_rate * 1000.0f);

    if (!device.open()) {
        fprintf(stderr, "Failed to open audio device\n");
        return 1;
    }

    DSPProgram* program = nullptr;
    CompileResult compile_result;
    bool using_compiled_graph = false;

    if (dpt_file) {
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
        fprintf(stderr, "\nNo .dpt file specified, using passthrough\n");

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
