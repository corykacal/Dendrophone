// Test program for .dpt parser
// Build: g++ -std=c++20 -I.. test_parser.cpp ../graph/dpt_parser.cpp -o test_parser

#include "../graph/dpt_parser.h"
#include <cstdio>

void print_graph(const Graph& g) {
    printf("Version: %d\n", g.version);

    printf("Audio inputs: ");
    for (const auto& inp : g.audio_inputs) printf("%s ", inp.c_str());
    printf("\n");

    printf("Audio outputs: ");
    for (const auto& out : g.audio_outputs) printf("%s ", out.c_str());
    printf("\n");

    printf("\nNodes:\n");
    for (const auto& [id, node] : g.nodes) {
        printf("  %s (type: %s)\n", id.c_str(), node.type.c_str());
        if (!node.inputs.empty()) {
            printf("    inputs: ");
            for (const auto& inp : node.inputs) printf("%s ", inp.c_str());
            printf("\n");
        }
        if (!node.outputs.empty()) {
            printf("    outputs: ");
            for (const auto& out : node.outputs) printf("%s ", out.c_str());
            printf("\n");
        }
        if (!node.params.empty()) {
            printf("    params:\n");
            for (const auto& [key, val] : node.params) {
                printf("      %s = ", key.c_str());
                if (auto* v = std::get_if<int>(&val)) printf("%d", *v);
                else if (auto* v = std::get_if<float>(&val)) printf("%.2f", *v);
                else if (auto* v = std::get_if<bool>(&val)) printf("%s", *v ? "true" : "false");
                else if (auto* v = std::get_if<std::string>(&val)) printf("\"%s\"", v->c_str());
                printf("\n");
            }
        }
    }

    printf("\nConnections:\n");
    for (const auto& conn : g.connections) {
        printf("  %s:%s -> %s:%s\n",
               conn.from.node.c_str(), conn.from.port.c_str(),
               conn.to.node.c_str(), conn.to.port.c_str());
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <file.dpt>\n", argv[0]);
        return 1;
    }

    printf("Parsing: %s\n", argv[1]);
    printf("========================================\n");

    auto result = DptParser::parse_file(argv[1]);

    if (!result.warnings.empty()) {
        printf("\nWarnings:\n");
        for (const auto& w : result.warnings) {
            printf("  [WARN] %s\n", w.c_str());
        }
    }

    if (!result.errors.empty()) {
        printf("\nErrors:\n");
        for (const auto& e : result.errors) {
            printf("  [ERROR] %s\n", e.c_str());
        }
    }

    if (result.success) {
        printf("\nParse successful!\n\n");
        print_graph(result.graph);
    } else {
        printf("\nParse FAILED.\n");
        return 1;
    }

    return 0;
}
