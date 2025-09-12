# Save this as: res://PlayerMovement.gd
extends CharacterBody2D

@onready var sprite = $Sprite2D

# Movement parameters
var marker_position: Vector3 = Vector3.ZERO
var screen_bounds: Rect2
var last_valid_position: Vector2

# Position mapping parameters - ADJUST THESE TO CONTROL SENSITIVITY
var position_scale: Vector2 = Vector2(1000, 1000)  # How much to scale marker movement
var position_offset: Vector2 = Vector2(400, 300)   # Screen center offset
var smoothing: float = 0.15                        # Movement smoothing (0-1)

func _ready():
	# Get screen bounds for clamping
	screen_bounds = get_viewport().get_visible_rect()
	position_offset = screen_bounds.get_center()  # Auto-center
	last_valid_position = global_position
	print("Player ready at position: ", global_position)

func update_from_marker_position(marker_pos: Vector3):
	marker_position = marker_pos
	
	# Only move if we have valid marker data
	if marker_pos.length() > 0.001:  # Small threshold to ignore tiny movements
		# Convert 3D marker position to 2D screen position
		var target_position = Vector2(
			marker_pos.x * position_scale.x + position_offset.x,
			-marker_pos.z * position_scale.y + position_offset.y  # Negative Z for proper Y movement
		)
		
		# Clamp to screen bounds with some padding
		var padding = 50
		target_position.x = clamp(target_position.x, padding, screen_bounds.size.x - padding)
		target_position.y = clamp(target_position.y, padding, screen_bounds.size.y - padding)
		
		# Smooth movement to target position
		global_position = global_position.lerp(target_position, smoothing)
		last_valid_position = global_position

func _process(_delta):
	# Update player color based on tracking status
	if sprite:
		if marker_position.length() > 0.001:
			# Active tracking - green tint
			sprite.modulate = Color.GREEN
		else:
			# No tracking - red tint  
			sprite.modulate = Color.RED

# Functions to adjust mapping during runtime
func set_position_scale(scale: Vector2):
	position_scale = scale
	print("Position scale set to: ", scale)

func set_smoothing(smooth_value: float):
	smoothing = clamp(smooth_value, 0.0, 1.0)
	print("Smoothing set to: ", smoothing)

func calibrate_center():
	position_offset = screen_bounds.get_center()
	print("Position calibrated to screen center: ", position_offset)
