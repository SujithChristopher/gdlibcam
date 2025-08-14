extends Control

@onready var start_button = $VBoxContainer/ButtonContainer/StartButton
@onready var stop_button = $VBoxContainer/ButtonContainer/StopButton
@onready var video_toggle_button = $VBoxContainer/ButtonContainer/VideoToggleButton
@onready var video_rect = $VBoxContainer/VideoContainer/VideoRect
@onready var detection_label = $VBoxContainer/VideoContainer/DetectionLabel

@onready var apriltag_detector: AprilTagDetector
var is_running = false
var video_enabled = false

# FPS tracking variables
var frame_count = 0
var fps_timer = 0.0
var current_fps = 0.0
var last_fps_update = 0.0

func _ready():
	
	print('hello there')
	# Initialize the AprilTag detector
	apriltag_detector = AprilTagDetector.new()
	
	# Load camera parameters using absolute path
	var json_path = "/home/sujith/Documents/games/gdlibcam/project/camera_parameters.json"
	if not apriltag_detector.load_camera_parameters(json_path):
		detection_label.text = "Failed to load camera parameters from: " + json_path
		return
	
	# Set marker size (5cm)
	apriltag_detector.set_marker_size(0.05)
	
	# Connect button signals
	start_button.pressed.connect(_on_start_pressed)
	stop_button.pressed.connect(_on_stop_pressed)
	video_toggle_button.pressed.connect(_on_video_toggle_pressed)
	
	# Initialize camera
	if not apriltag_detector.initialize_camera():
		detection_label.text = "Failed to initialize camera!"
		return
	
	detection_label.text = "Camera initialized. Ready to start detection."
	stop_button.disabled = true
	video_toggle_button.disabled = true

func _on_start_pressed():
	if not is_running:
		if apriltag_detector.start_camera():
			is_running = true
			start_button.disabled = true
			stop_button.disabled = false
			video_toggle_button.disabled = false
			detection_label.text = "Camera started. Looking for AprilTags..."
			
			# Reset FPS tracking
			frame_count = 0
			fps_timer = 0.0
			current_fps = 0.0
			
			print("Camera started successfully")
		else:
			detection_label.text = "Failed to start camera!"

func _on_stop_pressed():
	if is_running:
		apriltag_detector.stop_camera()
		is_running = false
		start_button.disabled = false
		stop_button.disabled = true
		video_toggle_button.disabled = true
		# Disable video feedback when camera stops
		if video_enabled:
			_on_video_toggle_pressed()
		detection_label.text = "Camera stopped."
		print("Camera stopped")

func _process(_delta):
	if is_running:
		# FPS tracking
		frame_count += 1
		fps_timer += _delta
		
		# Update FPS every second
		if fps_timer >= 1.0:
			current_fps = frame_count / fps_timer
			frame_count = 0
			fps_timer = 0.0
			last_fps_update = Time.get_ticks_msec() / 1000.0
		
		# Only update UI every 5th frame to reduce overhead
		if frame_count % 5 == 0:
			update_detection_display()
			
		# Update video feed if enabled (much less frequently for performance)
		if video_enabled and frame_count % 15 == 0:  # Only update every 15th frame (~4 FPS for video)
			update_video_display()

func update_detection_display():
	# Get latest detections from the detector
	var detections = apriltag_detector.get_latest_detections()
	
	var text = "FPS: %.1f\n\n" % current_fps
	
	if detections.size() > 0:
		text += "Detected " + str(detections.size()) + " marker(s):\n"
		
		for detection in detections:
			var id = detection["id"]
			var rvec = detection["rvec"]
			var tvec = detection["tvec"]
			
			text += "ID %d:\n" % id
			text += "  rvec: [%.3f, %.3f, %.3f]\n" % [rvec.x, rvec.y, rvec.z]
			text += "  tvec: [%.3f, %.3f, %.3f]\n" % [tvec.x, tvec.y, tvec.z]
			text += "  Distance: %.2f cm\n" % (tvec.length() * 100)
			text += "\n"
		
		detection_label.text = text
	else:
		text += "Camera running. No markers detected."
		detection_label.text = text

func _on_video_toggle_pressed():
	video_enabled = not video_enabled
	apriltag_detector.set_video_feedback_enabled(video_enabled)
	
	if video_enabled:
		video_toggle_button.text = "Disable Video"
		video_rect.show()
	else:
		video_toggle_button.text = "Enable Video"
		video_rect.hide()
		video_rect.texture = null

func update_video_display():
	if video_enabled and apriltag_detector:
		var texture = apriltag_detector.get_current_frame_texture()
		if texture and texture.get_image() and not texture.get_image().is_empty():
			video_rect.texture = texture
		else:
			# No valid frame yet, keep waiting
			pass

func _exit_tree():
	# Clean up when exiting
	if apriltag_detector and is_running:
		apriltag_detector.stop_camera()
