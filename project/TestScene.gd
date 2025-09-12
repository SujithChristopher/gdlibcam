# Save this as: res://TestScene.gd
extends Node2D

@onready var player: Player
@onready var ui_container: Control
@onready var position_label: Label
@onready var marker_tracker: MarkerTracker
@onready var apriltag_detector: AprilTagDetector

var is_camera_running: bool = false

func _ready():
	setup_scene()
	setup_ui()
	setup_marker_detection()

func setup_scene():
	# Create player
	player = preload("res://Player.tscn").instantiate() as Player
	if not player:
		# If scene doesn't exist, create player programmatically
		player = Player.new()
	add_child(player)
	
	# Position player in center initially
	player.global_position = get_viewport().get_visible_rect().get_center()

func setup_ui():
	# Create UI overlay
	ui_container = Control.new()
	ui_container.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(ui_container)
	
	# Create position display label
	position_label = Label.new()
	position_label.position = Vector2(10, 10)
	position_label.size = Vector2(300, 100)
	position_label.text = "Marker Position: Not detected"
	ui_container.add_child(position_label)
	
	# Create control buttons
	create_control_buttons()

func create_control_buttons():
	var button_container = VBoxContainer.new()
	button_container.position = Vector2(10, 120)
	ui_container.add_child(button_container)
	
	# Start/Stop button
	var toggle_button = Button.new()
	toggle_button.text = "Start Camera"
	toggle_button.pressed.connect(_on_toggle_camera)
	button_container.add_child(toggle_button)
	
	# Reset tracking button
	var reset_button = Button.new()
	reset_button.text = "Reset Tracking"
	reset_button.pressed.connect(_on_reset_tracking)
	button_container.add_child(reset_button)
	
	# Calibration button
	var calibrate_button = Button.new()
	calibrate_button.text = "Calibrate Position"
	calibrate_button.pressed.connect(_on_calibrate_position)
	button_container.add_child(calibrate_button)

func setup_marker_detection():
	# Initialize marker tracker
	marker_tracker = MarkerTracker.new()
	add_child(marker_tracker)
	
	# Initialize AprilTag detector (don't add as child if it's not a Node)
	apriltag_detector = AprilTagDetector.new()
	# Only add as child if it inherits from Node, otherwise just keep the reference
	
	# Load camera parameters - UPDATED PATH TO MATCH YOUR SYSTEM
	var json_path = "/home/sujith/Documents/games/gdlibcam/project/camera_parameters.json"
	if not apriltag_detector.load_camera_parameters(json_path):
		print("Failed to load camera parameters")
		position_label.text = "Failed to load camera parameters!"
		return
	
	apriltag_detector.set_marker_size(0.05)
	
	if not apriltag_detector.initialize_camera():
		print("Failed to initialize camera")
		position_label.text = "Failed to initialize camera!"
		return
	
	print("Camera system ready")

func _process(_delta):
	if is_camera_running:
		# Get detections from C++ extension
		var detections = apriltag_detector.get_latest_detections()
		
		# Process through marker tracker
		var filtered_position = marker_tracker.process_detections(detections)
		
		# Update player position
		if player:
			player.update_from_marker_position(filtered_position)
		
		# Update UI
		update_position_display(filtered_position, detections)

func update_position_display(position: Vector3, detections: Array):
	var text = "Marker Position: [%.3f, %.3f, %.3f]\n" % [position.x, position.y, position.z]
	text += "Detected Markers: %d\n" % detections.size()
	
	if detections.size() > 0:
		text += "Active IDs: "
		for detection in detections:
			text += str(detection["id"]) + " "
	
	position_label.text = text

func _on_toggle_camera():
	if not is_camera_running:
		if apriltag_detector.start_camera():
			is_camera_running = true
			marker_tracker.start_recording("TEST_PATIENT")
			print("Camera started")
			# Update button text through UI
			var toggle_button = ui_container.get_child(1).get_child(0) as Button
			if toggle_button:
				toggle_button.text = "Stop Camera"
		else:
			print("Failed to start camera")
	else:
		apriltag_detector.stop_camera()
		marker_tracker.stop_recording()
		is_camera_running = false
		print("Camera stopped")
		# Update button text
		var toggle_button = ui_container.get_child(1).get_child(0) as Button
		if toggle_button:
			toggle_button.text = "Start Camera"

func _on_reset_tracking():
	if marker_tracker:
		marker_tracker.reset_tracking()
	print("Tracking reset")

func _on_calibrate_position():
	# Calibrate player mapping
	if player and player.has_method("calibrate_center"):
		player.calibrate_center()
		
	# Also reset marker tracker reference frame
	if marker_tracker:
		marker_tracker.reset_tracking()
	print("Position calibrated - Player will center on current marker position")

func _exit_tree():
	if apriltag_detector and is_camera_running:
		apriltag_detector.stop_camera()
	if marker_tracker:
		marker_tracker.stop_recording()
