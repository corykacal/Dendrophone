extends Node3D

@onready var nodes_container = $NodesContainer
@onready var edges_container = $EdgesContainer

var graph: GraphData.Graph = null
var node_positions: Dictionary = {}
var node_views: Dictionary = {}  # node_id -> NodeView instance

func _ready():
	print("GraphController ready")

	# Get file path from command line arguments
	var args = OS.get_cmdline_user_args()
	if args.size() == 0:
		print("Usage: godot --path . -- <path-to-dpt-file>")
		print("Example: godot --path . -- resources/test_graphs/passthrough.dpt")
		return

	var file_path = args[0]
	load_dpt(file_path)

func load_dpt(file_path: String):
	print("Loading DPT file: " + file_path)

	# Parse the DPT file
	var result = DPTLoader.parse_dpt(file_path)

	if not result.success:
		print("Failed to load DPT file:")
		for error in result.errors:
			print("  ERROR: " + error)
		return

	print("DPT file parsed successfully!")

	# Convert to graph data structures
	graph = GraphData.Graph.from_dpt_data(result.data)

	print("Graph loaded:")
	print("  Nodes: " + str(graph.nodes.size()))
	print("  Connections: " + str(graph.connections.size()))

	# Calculate layout
	node_positions = GraphLayout.calculate_positions(graph)

	print("Layout calculated:")
	for node_id in node_positions:
		var pos = node_positions[node_id]
		print("  " + node_id + ": " + str(pos))

	# Spawn visual representations (will implement next)
	spawn_visuals()

func spawn_visuals():
	# Clear existing visuals
	for child in nodes_container.get_children():
		child.queue_free()
	for child in edges_container.get_children():
		child.queue_free()

	node_views.clear()

	# Load NodeView scene
	var node_view_scene = preload("res://scenes/node_view.tscn")

	# Spawn nodes
	for node_id in graph.nodes:
		var graph_node = graph.nodes[node_id]
		var node_view = node_view_scene.instantiate()

		# Setup node data
		node_view.setup(graph_node)

		# Position node
		if node_positions.has(node_id):
			node_view.position = node_positions[node_id]

		nodes_container.add_child(node_view)
		node_views[node_id] = node_view

	print("Spawned " + str(graph.nodes.size()) + " nodes")

	# Spawn edges
	spawn_edges()

func spawn_edges():
	# Create EdgeView for each connection
	for conn in graph.connections:
		var from_node_id = conn.from.node
		var to_node_id = conn.to.node

		if not node_views.has(from_node_id) or not node_views.has(to_node_id):
			print("Warning: Connection references unknown node")
			continue

		var from_node = node_views[from_node_id]
		var to_node = node_views[to_node_id]

		# Check if this is a parameter connection
		var is_param = GraphLayout.is_parameter_connection(conn, graph)

		# Create edge
		var edge_view = EdgeView.new()
		edge_view.setup(from_node, to_node, conn.from.port, conn.to.port, is_param)

		edges_container.add_child(edge_view)

	print("Spawned " + str(graph.connections.size()) + " edges")
