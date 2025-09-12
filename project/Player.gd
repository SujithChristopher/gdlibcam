# Save this as: res://Player.gd
extends CharacterBody2D
class_name Player

@onready var sprite = $Sprite2D
@onready var collision = $CollisionShape2D

# Movement parameters
var movement_speed: float = 200.0
var position_smoothing: float = 0.1
var marker_position: Vector3 = Vector3.ZERO
var screen_bounds: Rect2

# Position mapping parameters
var position_scale: Vector2 = Vector2(500, 500)  # Scale marker coords to screen
var position_offset: Vector2 = Vector2(400, 300)  # Center offset

func _ready():
	# Set up basic player appearance
	setup_player_visual()
	
	# Get screen bounds for clamping
	screen_bounds = get_viewport().get_visible_rect()
	
	# Connect to marker tracking updates (we'll implement this)
	connect_to_marker_tracker()

func setup_player_visual():
	# Create a simple colored rectangle as player
	if not sprite:
		sprite = Sprite2D.new()
		add_child(sprite)
	
	# Create a simple colored texture
	var image = Image.create(32, 32, false, Image.FORMAT_RGB8)
	image.fill(Color.CYAN)  # Cyan colored player
	var texture = ImageTexture.new()
	texture.set_image(image)
	sprite.texture = texture
	
	# Set up collision
	if not collision:
		collision = CollisionShape2D.new()
		add_child(collision)
	
	var shape = RectangleShape2D.new()
	shape.size = Vector2(32, 32)
	collision.shape = shape

func connect_to_marker_tracker():
	# We'll get the position from the main scene's marker tracker
	# This will be called from the main scene
	pass

func update_from_marker_position(marker_pos: Vector3):
	marker_position = marker_pos
	
	# Convert 3D marker position to 2D screen position
	var target_position = Vector2(
		marker_pos.x * position_scale.x + position_offset.x,
		marker_pos.z * position_scale.y + position_offset.y  # Use Z for Y movement
	)
	
	# Clamp to screen bounds
	target_position.x = clamp(target_position.x, 0, screen_bounds.size.x)
	target_position.y = clamp(target_position.y, 0, screen_bounds.size.y)
	
	# Smooth movement to target position
	global_position = global_position.lerp(target_position, position_smoothing)

func _process(_delta):
	# Update player color based on movement
	if marker_position.length() > 0:
		# Active tracking - green tint
		sprite.modulate = Color.GREEN.lerp(Color.WHITE, 0.7)
	else:
		# No tracking - red tint
		sprite.modulate = Color.RED.lerp(Color.WHITE, 0.7)

func set_position_mapping(scale: Vector2, offset: Vector2):
	position_scale = scale
	position_offset = offset

func set_smoothing(smoothing_value: float):
	position_smoothing = clamp(smoothing_value, 0.0, 1.0)
