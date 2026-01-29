# Dendrophone Audio Engine Makefile
# For systems without CMake

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3 -ffast-math
LDFLAGS = -lasound -lpthread

SRCDIR = .
BUILDDIR = build

SOURCES = io/alsa_device.cpp engine/audio_engine.cpp graph/dpt_parser.cpp graph/graph_normalizer.cpp graph/graph_compiler.cpp effects/delay.cpp effects/lfo.cpp effects/envelope.cpp effects/reverb.cpp effects/pitch_shift.cpp effects/svfilter.cpp effects/reverse.cpp effects/multitap_delay.cpp effects/granular.cpp effects/looper.cpp effects/glitch.cpp effects/chorus.cpp effects/tremolo.cpp effects/flanger.cpp effects/compressor.cpp effects/overdrive.cpp effects/autowah.cpp main.cpp
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
$(BUILDDIR)/graph/graph_compiler.o: graph/graph_compiler.cpp graph/graph_compiler.h graph/graph_ast.h core/dsp_program_builder.h effects/delay.h effects/lfo.h effects/envelope.h effects/reverb.h effects/pitch_shift.h effects/svfilter.h effects/reverse.h effects/multitap_delay.h effects/granular.h effects/looper.h effects/glitch.h effects/chorus.h effects/tremolo.h effects/flanger.h effects/compressor.h effects/overdrive.h effects/autowah.h
$(BUILDDIR)/effects/delay.o: effects/delay.cpp effects/delay.h core/dsp_block.h
$(BUILDDIR)/effects/lfo.o: effects/lfo.cpp effects/lfo.h core/dsp_block.h
$(BUILDDIR)/effects/envelope.o: effects/envelope.cpp effects/envelope.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/reverb.o: effects/reverb.cpp effects/reverb.h core/dsp_block.h
$(BUILDDIR)/effects/pitch_shift.o: effects/pitch_shift.cpp effects/pitch_shift.h core/dsp_block.h
$(BUILDDIR)/effects/svfilter.o: effects/svfilter.cpp effects/svfilter.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/reverse.o: effects/reverse.cpp effects/reverse.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/multitap_delay.o: effects/multitap_delay.cpp effects/multitap_delay.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/granular.o: effects/granular.cpp effects/granular.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/looper.o: effects/looper.cpp effects/looper.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/glitch.o: effects/glitch.cpp effects/glitch.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/chorus.o: effects/chorus.cpp effects/chorus.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/tremolo.o: effects/tremolo.cpp effects/tremolo.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/flanger.o: effects/flanger.cpp effects/flanger.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/compressor.o: effects/compressor.cpp effects/compressor.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/overdrive.o: effects/overdrive.cpp effects/overdrive.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/effects/autowah.o: effects/autowah.cpp effects/autowah.h core/dsp_block.h core/dsp_param.h
$(BUILDDIR)/main.o: main.cpp io/alsa_device.h engine/audio_engine.h core/dsp_program_builder.h core/dsp_ops.h
