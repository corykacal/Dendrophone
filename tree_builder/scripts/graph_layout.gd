extends Node

class_name GraphLayout

# Layout constants (converted from pixels to 3D units)
const LAYER_SPACING = 4.0
const NODE_SPACING = 2.5
const START_X = -10.0
const START_Y = 0.0
const LFO_Y = -8.0
const LFO_SPACING = 3.5

static func calculate_positions(graph: GraphData.Graph) -> Dictionary:
	"""
	Calculate 3D positions for all nodes in the graph.
	Returns: Dictionary mapping node_id -> Vector3
	"""
	if graph.nodes.is_empty():
		return {}

	# Separate LFO/envelope nodes from signal flow nodes
	var lfo_nodes: Array[String] = []
	var signal_nodes: Array[String] = []

	for node_id in graph.nodes:
		var node = graph.nodes[node_id]
		if node.type == "lfo" or node.type == "envelope":
			lfo_nodes.append(node_id)
		else:
			signal_nodes.append(node_id)

	# Build adjacency map for layer calculation (signal nodes only)
	var outgoing: Dictionary = {}  # node_id -> Array[String] of connected nodes
	var incoming_count: Dictionary = {}  # node_id -> int

	for node_id in signal_nodes:
		outgoing[node_id] = []
		incoming_count[node_id] = 0

	# Only count signal edges (not parameter connections from LFOs)
	for conn in graph.connections:
		var from_node = conn.from.node
		var to_node = conn.to.node

		# Skip if edge is from an LFO (parameter modulation)
		if from_node in lfo_nodes:
			continue

		# Only process if both nodes are signal nodes
		if outgoing.has(from_node) and incoming_count.has(to_node):
			if to_node not in outgoing[from_node]:
				outgoing[from_node].append(to_node)
			incoming_count[to_node] += 1

	# Assign layers using BFS
	var node_layer: Dictionary = {}  # node_id -> layer index
	var queue: Array[String] = []

	# Start with nodes that have no incoming edges
	for node_id in signal_nodes:
		if incoming_count[node_id] == 0:
			node_layer[node_id] = 0
			queue.append(node_id)

	# BFS traversal
	while not queue.is_empty():
		var current = queue.pop_front()
		var current_layer = node_layer[current]

		for next_node in outgoing[current]:
			var proposed_layer = current_layer + 1
			if not node_layer.has(next_node) or node_layer[next_node] < proposed_layer:
				node_layer[next_node] = proposed_layer

			if next_node not in queue:
				queue.append(next_node)

	# Handle isolated nodes (assign to layer 0)
	for node_id in signal_nodes:
		if not node_layer.has(node_id):
			node_layer[node_id] = 0

	# Group signal nodes by layer
	var layers: Dictionary = {}  # layer_index -> Array[String] of node_ids
	for node_id in signal_nodes:
		var layer = node_layer[node_id]
		if not layers.has(layer):
			layers[layer] = []
		layers[layer].append(node_id)

	# Position signal flow nodes
	var positions: Dictionary = {}

	for layer in layers:
		var nodes_in_layer = layers[layer]
		var layer_height = (nodes_in_layer.size() - 1) * NODE_SPACING

		for i in range(nodes_in_layer.size()):
			var x = START_X + layer * LAYER_SPACING
			var y = START_Y + i * NODE_SPACING - layer_height / 2.0
			var z = 0.0
			positions[nodes_in_layer[i]] = Vector3(x, y, z)

	# Position LFO nodes at the bottom in a row
	for i in range(lfo_nodes.size()):
		var x = START_X + i * LFO_SPACING
		var y = LFO_Y
		var z = 0.0
		positions[lfo_nodes[i]] = Vector3(x, y, z)

	return positions

static func is_parameter_connection(conn: GraphData.GraphConnection, graph: GraphData.Graph) -> bool:
	"""
	Check if a connection is a parameter modulation connection.
	Parameter connections are from LFO/envelope outputs to param_inputs of other nodes.
	"""
	var from_node_id = conn.from.node
	var to_node_id = conn.to.node
	var to_port = conn.to.port

	if not graph.nodes.has(from_node_id) or not graph.nodes.has(to_node_id):
		return false

	var from_node = graph.nodes[from_node_id]
	var to_node = graph.nodes[to_node_id]

	# Check if from node is LFO/envelope
	var is_from_control = from_node.type == "lfo" or from_node.type == "envelope"

	# Check if to port is a param_input
	var is_to_param = to_port in to_node.param_inputs

	return is_from_control and is_to_param
