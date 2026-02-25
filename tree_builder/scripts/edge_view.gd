extends MeshInstance3D

class_name EdgeView

var from_node: Node3D = null
var to_node: Node3D = null
var from_port: String = ""
var to_port: String = ""
var is_parameter_connection: bool = false

# Edge colors
const AUDIO_COLOR = Color(0.3, 0.6, 1.0)      # Blue
const PARAMETER_COLOR = Color(1.0, 0.7, 0.2)  # Orange

# Curve parameters
const CURVE_SEGMENTS = 20
const CURVE_CONTROL_DISTANCE = 1.5

var immediate_mesh: ImmediateMesh

func _ready():
	immediate_mesh = ImmediateMesh.new()
	mesh = immediate_mesh

	# Create material
	var mat = StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.vertex_color_use_as_albedo = true
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material_override = mat

	update_curve()

func setup(from: Node3D, to: Node3D, from_p: String, to_p: String, is_param: bool):
	"""Initialize edge with node references and port names"""
	from_node = from
	to_node = to
	from_port = from_p
	to_port = to_p
	is_parameter_connection = is_param

	if is_node_ready():
		update_curve()

func update_curve():
	"""Redraw the edge curve"""
	if not from_node or not to_node:
		return

	if not immediate_mesh:
		return

	immediate_mesh.clear_surfaces()

	var from_pos = from_node.global_position + _get_port_offset(from_node, from_port, true)
	var to_pos = to_node.global_position + _get_port_offset(to_node, to_port, false)

	# Bezier control points for smooth curve
	var ctrl1 = from_pos + Vector3(CURVE_CONTROL_DISTANCE, 0, 0)
	var ctrl2 = to_pos + Vector3(-CURVE_CONTROL_DISTANCE, 0, 0)

	# Choose color based on connection type
	var color = PARAMETER_COLOR if is_parameter_connection else AUDIO_COLOR

	# Draw curve
	immediate_mesh.surface_begin(Mesh.PRIMITIVE_LINE_STRIP)

	for i in range(CURVE_SEGMENTS):
		var t = float(i) / float(CURVE_SEGMENTS - 1)
		var pos = _cubic_bezier(from_pos, ctrl1, ctrl2, to_pos, t)
		immediate_mesh.surface_set_color(color)
		immediate_mesh.surface_add_vertex(pos)

	immediate_mesh.surface_end()

func _get_port_offset(node: Node3D, port_name: String, is_output: bool) -> Vector3:
	"""Calculate offset from node center to port position"""
	# For now, simple offset to left (inputs) or right (outputs) of node
	var offset_x = 1.0 if is_output else -1.0
	return Vector3(offset_x, 0, 0)

func _cubic_bezier(p0: Vector3, p1: Vector3, p2: Vector3, p3: Vector3, t: float) -> Vector3:
	"""Calculate point on cubic Bezier curve"""
	var u = 1.0 - t
	var tt = t * t
	var uu = u * u
	var uuu = uu * u
	var ttt = tt * t

	var p = p0 * uuu
	p += p1 * 3.0 * uu * t
	p += p2 * 3.0 * u * tt
	p += p3 * ttt

	return p

func _process(_delta):
	# Update curve if nodes have moved
	if from_node and to_node:
		update_curve()
