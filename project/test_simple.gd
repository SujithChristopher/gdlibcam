extends Control

@onready var label = $Label

func _ready():
	print("Starting simple test...")
	label.text = "Testing GDExtension..."
	
	# Test 1: Try to create the class
	print("Creating AprilTagDetector...")
	var detector = AprilTagDetector.new()
	if detector:
		print("SUCCESS: AprilTagDetector created")
		label.text = "AprilTagDetector created successfully!"
	else:
		print("FAILED: Could not create AprilTagDetector")
		label.text = "Failed to create AprilTagDetector"
		return
	
	# Test 2: Try to load camera parameters
	print("Loading camera parameters...")
	var json_path = "/home/sujith/Documents/games/cpptest/project/camera_parameters.json"
	if detector.load_camera_parameters(json_path):
		print("SUCCESS: Camera parameters loaded")
		label.text = "Camera parameters loaded successfully!"
	else:
		print("FAILED: Could not load camera parameters")
		label.text = "Failed to load camera parameters"
		return
	
	print("All tests passed!")
