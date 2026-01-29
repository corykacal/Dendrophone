# Dendrophone Audio Engine Makefile
# For systems without CMake

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3 -ffast-math
LDFLAGS = -lasound -lpthread

SRCDIR = .
BUILDDIR = build

SOURCES = io/alsa_device.cpp engine/audio_engine.cpp graph/dpt_parser.cpp graph/graph_normalizer.cpp graph/graph_compiler.cpp effects/delay.cpp effects/lfo.cpp main.cpp
OBJECTS = $(SOURCES:%.cpp=$(BUILDDIR)/%.o)
TARGET = $(BUILDDIR)/dendrophone

.PHONY: all clean debug

all: $(TARGET)

debug: CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -g -O0
debug: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

# Dependencies
$(BUILDDIR)/io/alsa_device.o: io/alsa_device.cpp io/alsa_device.h io/audio_device.h
$(BUILDDIR)/engine/audio_engine.o: engine/audio_engine.cpp engine/audio_engine.h io/audio_device.h core/dsp_program.h
$(BUILDDIR)/graph/dpt_parser.o: graph/dpt_parser.cpp graph/dpt_parser.h graph/graph_ast.h
$(BUILDDIR)/graph/graph_normalizer.o: graph/graph_normalizer.cpp graph/graph_normalizer.h graph/graph_ast.h
$(BUILDDIR)/graph/graph_compiler.o: graph/graph_compiler.cpp graph/graph_compiler.h graph/graph_ast.h core/dsp_program_builder.h effects/delay.h
$(BUILDDIR)/effects/delay.o: effects/delay.cpp effects/delay.h core/dsp_block.h
$(BUILDDIR)/main.o: main.cpp io/alsa_device.h engine/audio_engine.h core/dsp_program_builder.h core/dsp_ops.h
