extends Node3D

class_name NodeView

@export var node_id: String = ""
@export var node_type: String = ""
@export var node_inputs: Array[String] = []
@export var node_outputs: Array[String] = []
@export var node_param_inputs: Array[String] = []
@export var node_params: Dictionary = {}

@onready var body_mesh = $BodyMesh
@onready var id_label = $IDLabel
@onready var type_label = $TypeLabel
@onready var ports_container = $PortsContainer

# Node color scheme
const NODE_COLORS = {
	"input": Color(0.3, 0.8, 0.3),      # Green
	"output": Color(0.8, 0.3, 0.3),     # Red
	"lfo": Color(0.3, 0.5, 0.9),        # Blue
	"envelope": Color(0.8, 0.6, 0.3),   # Orange
	"default": Color(0.5, 0.5, 0.6)     # Gray
}

func _ready():
	update_appearance()

func setup(graph_node: GraphData.GraphNode):
	"""Initialize node from GraphNode data"""
	node_id = graph_node.id
	node_type = graph_node.type
	node_inputs = graph_node.inputs.duplicate()
	node_outputs = graph_node.outputs.duplicate()
	node_param_inputs = graph_node.param_inputs.duplicate()
	node_params = graph_node.params.duplicate()

	if is_node_ready():
		update_appearance()

func update_appearance():
	"""Update visual appearance based on node properties"""
	if id_label:
		id_label.text = node_id
	if type_label:
		type_label.text = node_type

	# Set color based on node type
	var color = NODE_COLORS.get(node_type, NODE_COLORS["default"])

	if body_mesh and body_mesh.get_surface_override_material_count() > 0:
		var mat = body_mesh.get_surface_override_material(0)
		if mat:
			mat.albedo_color = color

	# Create port indicators
	if ports_container:
		create_port_indicators()

func create_port_indicators():
	"""Create visual indicators for ports"""
	# Clear existing ports
	for child in ports_container.get_children():
		child.queue_free()

	var port_radius = 0.1
	var node_width = 2.0
	var node_height = 1.5

	# Create sphere mesh for ports
	var sphere_mesh = SphereMesh.new()
	sphere_mesh.radius = port_radius
	sphere_mesh.height = port_radius * 2

	# Input ports (left side)
	var input_spacing = node_height / (node_inputs.size() + 1) if node_inputs.size() > 0 else 0
	for i in range(node_inputs.size()):
		var port_indicator = MeshInstance3D.new()
		port_indicator.mesh = sphere_mesh

		var mat = StandardMaterial3D.new()
		mat.albedo_color = Color(0.3, 0.8, 0.3)  # Green for inputs
		port_indicator.material_override = mat

		var y_pos = node_height / 2 - (i + 1) * input_spacing
		port_indicator.position = Vector3(-node_width / 2 - port_radius, y_pos, 0)

		ports_container.add_child(port_indicator)

	# Output ports (right side)
	var output_spacing = node_height / (node_outputs.size() + 1) if node_outputs.size() > 0 else 0
	for i in range(node_outputs.size()):
		var port_indicator = MeshInstance3D.new()
		port_indicator.mesh = sphere_mesh

		var mat = StandardMaterial3D.new()
		mat.albedo_color = Color(0.8, 0.3, 0.3)  # Red for outputs
		port_indicator.material_override = mat

		var y_pos = node_height / 2 - (i + 1) * output_spacing
		port_indicator.position = Vector3(node_width / 2 + port_radius, y_pos, 0)

		ports_container.add_child(port_indicator)

	# Parameter input ports (bottom)
	var param_spacing = node_width / (node_param_inputs.size() + 1) if node_param_inputs.size() > 0 else 0
	for i in range(node_param_inputs.size()):
		var port_indicator = MeshInstance3D.new()
		port_indicator.mesh = sphere_mesh

		var mat = StandardMaterial3D.new()
		mat.albedo_color = Color(1.0, 0.7, 0.2)  # Orange for param inputs
		port_indicator.material_override = mat

		var x_pos = -node_width / 2 + (i + 1) * param_spacing
		port_indicator.position = Vector3(x_pos, -node_height / 2 - port_radius, 0)

		ports_container.add_child(port_indicator)
