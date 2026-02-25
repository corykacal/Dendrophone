extends Node

class_name GraphData

# Mirrors the C++ graph_ast.h structures

class PortRef:
	var node: String
	var port: String

	func _init(node_id: String = "", port_name: String = ""):
		node = node_id
		port = port_name

	static func from_string(port_str: String) -> PortRef:
		var parts = port_str.split(":", false)
		if parts.size() != 2:
			push_error("Invalid port reference: " + port_str)
			return PortRef.new()
		return PortRef.new(parts[0], parts[1])

	func to_string() -> String:
		return node + ":" + port

class GraphConnection:
	var from: PortRef
	var to: PortRef

	func _init(from_port: PortRef = null, to_port: PortRef = null):
		from = from_port if from_port else PortRef.new()
		to = to_port if to_port else PortRef.new()

class GraphNode:
	var id: String
	var type: String
	var rate: String = "audio"  # "audio" or "control"
	var inputs: Array[String] = []
	var outputs: Array[String] = []
	var param_inputs: Array[String] = []
	var params: Dictionary = {}

	func _init(node_id: String = ""):
		id = node_id

class Graph:
	var version: int = 1
	var audio_inputs: Array[String] = []
	var audio_outputs: Array[String] = []
	var nodes: Dictionary = {}  # node_id -> GraphNode
	var connections: Array[GraphConnection] = []

	static func from_dpt_data(data: Dictionary) -> Graph:
		var graph = Graph.new()

		graph.version = data.get("dpt_version", 1)

		# Parse audio I/O
		if data.has("audio"):
			var audio = data.audio
			if audio.has("inputs"):
				graph.audio_inputs.assign(audio.inputs)
			if audio.has("outputs"):
				graph.audio_outputs.assign(audio.outputs)

		# Parse nodes
		if data.has("nodes"):
			for node_id in data.nodes:
				var node_data = data.nodes[node_id]
				var node = GraphNode.new(node_id)

				node.type = node_data.get("type", "")
				node.rate = node_data.get("rate", "audio")

				if node_data.has("inputs"):
					node.inputs.assign(node_data.inputs)
				if node_data.has("outputs"):
					node.outputs.assign(node_data.outputs)
				if node_data.has("param_inputs"):
					node.param_inputs.assign(node_data.param_inputs)
				if node_data.has("params"):
					node.params = node_data.params.duplicate()

				graph.nodes[node_id] = node

		# Parse connections
		if data.has("connections"):
			for conn_data in data.connections:
				var from_ref = PortRef.from_string(conn_data.from)
				var to_ref = PortRef.from_string(conn_data.to)
				var conn = GraphConnection.new(from_ref, to_ref)
				graph.connections.append(conn)

		return graph
