# Dendrophone Audio Engine Makefile
# For systems without CMake

CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -O3 -ffast-math
LDFLAGS = -lasound -lpthread

SRCDIR = .
BUILDDIR = build

SOURCES = io/alsa_device.cpp engine/audio_engine.cpp main.cpp
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
$(BUILDDIR)/main.o: main.cpp io/alsa_device.h engine/audio_engine.h core/dsp_program_builder.h core/dsp_ops.h
