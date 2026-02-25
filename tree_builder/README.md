# DPT Tree Visualizer

An orthographic 3D visualizer for .dpt (Dendrophone Processing Topology) audio processing graphs, built with Godot 4.6.

## Overview

This tool visualizes audio processing graphs defined in .dpt files with a clean, Wii U/3DS-style orthographic 3D interface. Nodes are automatically laid out using a BFS algorithm, with signal flow proceeding from left (input) to right (output) in invisible vertical layers.

## Features

- **Orthographic 3D visualization** at 45° elevation angle
- **Automatic graph layout** using BFS topological sorting
- **Left-to-right signal flow** with layered organization
- **Color-coded nodes** by type (input=green, output=red, LFO=blue, envelope=orange)
- **Bezier curve edges** for smooth connections
- **Parameter connection highlighting** (audio=blue, parameter=orange)
- **Port indicators** showing inputs, outputs, and parameter inputs
- **Camera controls** for navigation (pan, zoom, reset)

## Requirements

- Godot 4.6 or later
- macOS, Linux, or Windows

## Usage

### Running from Command Line

```bash
godot --path /path/to/tree_builder -- resources/test_graphs/passthrough.dpt
```

Or from within the tree_builder directory:

```bash
godot --path . -- resources/test_graphs/passthrough.dpt
```

### Running in Godot Editor

1. Open the project in Godot Editor
2. Set the command line arguments in Project Settings or run configuration
3. Press F5 to run

### Camera Controls

- **Pan**: WASD keys
- **Zoom**: Mouse scroll wheel
- **Reset**: R key

## DPT File Format

DPT files are JSON-based and describe audio processing graphs:

```json
{
  "dpt_version": 1,
  "audio": {
    "inputs": ["L", "R"],
    "outputs": ["L", "R"]
  },
  "nodes": {
    "node_id": {
      "type": "effect_type",
      "rate": "audio"|"control",
      "inputs": ["port_name"],
      "outputs": ["port_name"],
      "param_inputs": ["param_name"],
      "params": {"key": value}
    }
  },
  "connections": [
    {"from": "node:port", "to": "node:port"}
  ]
}
```

### Special Node Types

- `input` - Graph entry point (far left)
- `output` - Graph exit point (far right)
- `lfo` - Low-frequency oscillator (positioned below signal flow)
- `envelope` - ADSR envelope (positioned below signal flow)

## Project Structure

```
tree_builder/
├── main.tscn                    # Main scene
├── scripts/
│   ├── camera_controller.gd    # Camera navigation
│   ├── dpt_loader.gd            # JSON parsing and validation
│   ├── graph_controller.gd     # Orchestration
│   ├── graph_data.gd            # Data structures
│   ├── graph_layout.gd         # BFS layout algorithm
│   ├── node_view.gd             # Node visualization
│   └── edge_view.gd             # Edge visualization
├── scenes/
│   └── node_view.tscn           # Node prefab
└── resources/
    └── test_graphs/             # 52 example .dpt files
```

## Testing

Test all .dpt files at once:

```bash
./test_all_dpts.sh
```

This will load each .dpt file in resources/test_graphs/ and verify it parses and renders correctly.

## Implementation Details

### Layout Algorithm

The visualizer uses a BFS (Breadth-First Search) algorithm to assign nodes to vertical layers:

1. **Node separation**: LFO/envelope nodes are separated from signal flow nodes
2. **Adjacency building**: Signal connections are mapped (parameter connections ignored for topology)
3. **Layer assignment**: Nodes are assigned to layers based on signal flow depth
4. **Position calculation**:
   - X-axis: layer index × LAYER_SPACING (left→right)
   - Y-axis: node index within layer × NODE_SPACING (centered)
   - Z-axis: 0 (flat layout)
5. **LFO positioning**: LFOs arranged in a horizontal row below the main graph

### Visual Design

- **Projection**: Orthographic (no perspective distortion)
- **Camera angle**: 45° elevation for depth perception
- **Node size**: 2.0 × 1.5 × 0.3 units (width × height × depth)
- **Materials**: Low metallic, high roughness, subtle emission
- **Edge curves**: 20-segment Bezier curves with control points

## Future Enhancements

- Interactive node selection and inspection
- Manual node positioning (drag and drop)
- Parameter editing UI
- Animated edges with flowing particles
- File picker UI for loading .dpt files
- Export rendered views
- Performance optimization for very large graphs (>500 nodes)

## License

Part of the Dendrophone project.
