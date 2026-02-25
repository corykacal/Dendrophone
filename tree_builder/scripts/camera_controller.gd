extends Camera3D

const PAN_SPEED = 0.15
const ZOOM_SPEED = 0.8
const MIN_ORTHO_SIZE = 2.0
const MAX_ORTHO_SIZE = 50.0

var target_position = Vector3.ZERO
var initial_position = Vector3.ZERO
var initial_ortho_size = 15.0

func _ready():
	initial_position = position
	initial_ortho_size = size
	target_position = Vector3.ZERO

func _input(event):
	# Zoom with scroll wheel
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			size = max(MIN_ORTHO_SIZE, size - ZOOM_SPEED)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			size = min(MAX_ORTHO_SIZE, size + ZOOM_SPEED)

func _process(_delta):
	# Pan with WASD
	var pan_input = Vector2.ZERO
	if Input.is_key_pressed(KEY_A):
		pan_input.x -= 1
	if Input.is_key_pressed(KEY_D):
		pan_input.x += 1
	if Input.is_key_pressed(KEY_W):
		pan_input.y += 1
	if Input.is_key_pressed(KEY_S):
		pan_input.y -= 1

	if pan_input.length() > 0:
		var right = transform.basis.x
		var up = transform.basis.y
		target_position += (right * pan_input.x + up * pan_input.y) * PAN_SPEED
		look_at(target_position, Vector3.UP)

	# Reset with R key
	if Input.is_key_pressed(KEY_R):
		target_position = Vector3.ZERO
		size = initial_ortho_size
		position = initial_position
		look_at(Vector3.ZERO, Vector3.UP)
