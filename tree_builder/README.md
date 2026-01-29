# Tree Builder - DPT Visualizer

A Qt Quick/QML-based visual editor for Dendrophone DPT (Dendrophone Processing Topology) files.

## Features

### MVP (Current)
- Load and visualize `.dpt` files
- 2D canvas with pan (drag) and zoom (scroll wheel)
- Nodes displayed as labeled boxes
- Edges displayed as connecting lines
- Dark theme UI

### Future
- 3D visual styling with Qt Quick 3D
- Open/save .dpt files via dialog
- Edit node parameters
- Drag nodes to reposition
- Add/remove nodes and edges
- Real-time validation
- Hot-swap integration with audio engine

## Build Instructions

### Prerequisites

**Raspberry Pi / Linux ARM64:**
```bash
sudo apt install qt6-base-dev qt6-declarative-dev cmake build-essential
```

**Linux x86_64:**
```bash
sudo apt install qt6-base-dev qt6-declarative-dev cmake build-essential
```

**Mac ARM:**
```bash
brew install qt@6 cmake
```

### Build

```bash
cd tree_builder
mkdir build && cd build

# On Linux
cmake ..

# On Mac
cmake -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@6 ..

# Build
make -j4
```

### Run

```bash
./tree_builder
```

The application will auto-load `resources/test_graphs/passthrough.dpt` on startup.

## Usage

- **Pan**: Click and drag anywhere on the canvas
- **Zoom**: Scroll wheel to zoom in/out
- **Nodes**: Boxes showing node ID and type
- **Edges**: Blue lines connecting nodes

## Architecture

```
C++ Backend (DPTModel)
    ↓ wraps
DPT Parser (from audio_engine)
    ↓ parses
.dpt JSON files
    ↓ exposes to
QML Frontend (Qt Quick)
    ↓ renders
Visual node graph
```

## File Structure

- `src/` - C++ backend (DPTModel, NodeModel)
- `qml/` - QML frontend (UI components)
- `resources/test_graphs/` - Test .dpt files
- `src/dendrophone/` - Symlink to audio_engine (DPT parser)

## Future Integration

The UI will eventually communicate with the audio engine daemon to:
- Send updated .dpt files
- Trigger hot-swap of compiled DSP programs
- Receive status/error feedback
- Control real-time parameters
