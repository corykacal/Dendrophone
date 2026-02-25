extends Node

class_name DPTLoader

static func parse_dpt(file_path: String) -> Dictionary:
	"""
	Parse a .dpt file and return structured data.
	Returns: {success: bool, data: Dictionary, errors: Array[String]}
	"""
	var result = {
		"success": false,
		"data": {},
		"errors": []
	}

	# Check if file exists
	if not FileAccess.file_exists(file_path):
		result.errors.append("File not found: " + file_path)
		return result

	# Read file
	var file = FileAccess.open(file_path, FileAccess.READ)
	if file == null:
		result.errors.append("Failed to open file: " + file_path)
		return result

	var json_str = file.get_as_text()
	file.close()

	# Parse JSON
	var json = JSON.new()
	var parse_result = json.parse(json_str)
	if parse_result != OK:
		result.errors.append("JSON parse error at line " + str(json.get_error_line()) + ": " + json.get_error_message())
		return result

	var data = json.data
	if typeof(data) != TYPE_DICTIONARY:
		result.errors.append("Root JSON must be a dictionary")
		return result

	# Validate structure
	var validation_errors = validate_graph(data)
	if validation_errors.size() > 0:
		result.errors = validation_errors
		return result

	# Success
	result.success = true
	result.data = data
	return result

static func validate_graph(data: Dictionary) -> Array[String]:
	"""
	Validate the parsed DPT data structure.
	Returns array of error messages (empty if valid).
	"""
	var errors: Array[String] = []

	# Check required top-level fields
	if not data.has("dpt_version"):
		errors.append("Missing required field: dpt_version")
	if not data.has("nodes"):
		errors.append("Missing required field: nodes")
	if not data.has("connections"):
		errors.append("Missing required field: connections")

	if errors.size() > 0:
		return errors

	# Check version
	if data.dpt_version != 1:
		errors.append("Unsupported dpt_version: " + str(data.dpt_version))

	# Validate nodes
	var nodes = data.nodes
	if typeof(nodes) != TYPE_DICTIONARY:
		errors.append("nodes must be a dictionary")
		return errors

	# Check for required input and output nodes
	var has_input = false
	var has_output = false
	for node_id in nodes:
		var node = nodes[node_id]
		if typeof(node) != TYPE_DICTIONARY:
			errors.append("Node '" + node_id + "' must be a dictionary")
			continue

		if not node.has("type"):
			errors.append("Node '" + node_id + "' missing required field: type")
			continue

		if node.type == "input":
			has_input = true
		elif node.type == "output":
			has_output = true

	if not has_input:
		errors.append("Graph must have at least one 'input' node")
	if not has_output:
		errors.append("Graph must have at least one 'output' node")

	# Validate connections
	var connections = data.connections
	if typeof(connections) != TYPE_ARRAY:
		errors.append("connections must be an array")
		return errors

	for i in range(connections.size()):
		var conn = connections[i]
		if typeof(conn) != TYPE_DICTIONARY:
			errors.append("Connection " + str(i) + " must be a dictionary")
			continue

		if not conn.has("from") or not conn.has("to"):
			errors.append("Connection " + str(i) + " missing 'from' or 'to' field")
			continue

		# Validate port references (format: "node_id:port_name")
		var from_parts = String(conn.from).split(":", false)
		var to_parts = String(conn.to).split(":", false)

		if from_parts.size() != 2:
			errors.append("Connection " + str(i) + " invalid 'from' format: " + str(conn.from))
		if to_parts.size() != 2:
			errors.append("Connection " + str(i) + " invalid 'to' format: " + str(conn.to))

		# Check if nodes exist
		if from_parts.size() == 2 and not nodes.has(from_parts[0]):
			errors.append("Connection " + str(i) + " references unknown node: " + from_parts[0])
		if to_parts.size() == 2 and not nodes.has(to_parts[0]):
			errors.append("Connection " + str(i) + " references unknown node: " + to_parts[0])

	return errors
