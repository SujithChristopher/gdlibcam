extends Node

signal SignalBus

@export var selected_training_hand: String = ""
@export var affected_hand: String = ""
@export var global_scalar_x: float = 1.0
@export var global_scalar_y: float = 1.0
@export var assessment_done: bool = false
@export var selected_game_mode: String = "2D"
@export var inflated_workspace: PackedVector2Array
@export var current_patient_id: String = ""
@export var data_path: String = OS.get_system_dir(OS.SYSTEM_DIR_DOCUMENTS) + "//NOARK//data"
@export var ball_position: Vector2

# ADD THESE NEW VARIABLES for marker tracking integration:
# These replace the network positions from your original UDP system
var network_position: Vector2 = Vector2.ZERO          # 2D game position
var network_position3D: Vector2 = Vector2.ZERO        # 3D game position  
var workspace: Vector2 = Vector2.ZERO                 # Workspace position

# Original hit detection variables
var hit_ground = Vector2.ZERO
var hit_top = Vector2.ZERO
var hit_left = Vector2.ZERO
var hit_right = Vector2.ZERO
var hit_player = Vector2.ZERO
var hit_computer = Vector2.ZERO

func enable_game_buttons(enable: bool) -> void:
	for button in get_tree().get_nodes_in_group("GameButtons"):
		button.disabled = not enable

# ADD THIS FUNCTION to update positions from marker tracking:
func update_marker_positions(marker_pos: Vector3):
	# Convert 3D marker position to your game coordinate systems
	# You can adjust these scaling factors based on your needs
	var scaled_x = marker_pos.x * global_scalar_x
	var scaled_y = marker_pos.y * global_scalar_y  
	var scaled_z = marker_pos.z * global_scalar_y
	
	# Update the position variables that your games expect
	network_position = Vector2(scaled_x, scaled_z)      # For 2D games
	network_position3D = Vector2(scaled_x, scaled_y)    # For 3D games
	workspace = Vector2(scaled_x, scaled_y)             # For workspace
	
	# Update ball position if needed for games
	ball_position = network_position
