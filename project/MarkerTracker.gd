# Save this as: res://MarkerTracker.gd
extends Node
class_name MarkerTracker

# Configuration constants (from Python Config class)
const MARKER_LENGTH = 0.05
const MARKER_SEPARATION = 0.01
const DEFAULT_IDS = [4, 8, 12, 14, 20]
const ALPHA = 0.4  # EMA filter smoothing factor

# Marker offsets (converted from Python numpy arrays to Vector3)
const MARKER_OFFSETS = {
	4: Vector3(0.0, 0.1, -0.069),
	8: Vector3(0.0, 0.01, -0.069),
	12: Vector3(0.0, 0.0, -0.1075),
	14: Vector3(-0.09, 0.0, -0.069),
	20: Vector3(0.1, 0.0, -0.069),
}

# EMA Filter class (converted from Python)
class ExponentialMovingAverageFilter3D:
	var alpha: float
	var ema_x: float
	var ema_y: float
	var ema_z: float
	var initialized: bool = false
	
	func _init(alpha_value: float):
		alpha = alpha_value
	
	func update(new_coords: Vector3) -> Vector3:
		if not initialized:
			ema_x = new_coords.x
			ema_y = new_coords.y
			ema_z = new_coords.z
			initialized = true
		else:
			ema_x = alpha * new_coords.x + (1.0 - alpha) * ema_x
			ema_y = alpha * new_coords.y + (1.0 - alpha) * ema_y
			ema_z = alpha * new_coords.z + (1.0 - alpha) * ema_z
		
		return Vector3(ema_x, ema_y, ema_z)

# Session and data management
var current_date: String = ""
var session_path: String = ""
var csv_file: FileAccess
var filter: ExponentialMovingAverageFilter3D
var record: bool = false

# First frame reference (converted from Python)
var first_frame: bool = true
var first_ids: Array = []
var first_rvecs: Array = []
var first_tvecs: Array = []

# Current patient info
var current_patient_id: String = ""

func _init():
	filter = ExponentialMovingAverageFilter3D.new(ALPHA)
	current_date = _get_date_string()

# Convert Python's rodrigues rotation to Godot
func _rodrigues_to_matrix(rvec: Vector3) -> Basis:
	# Convert rotation vector to rotation matrix
	var angle = rvec.length()
	if angle == 0:
		return Basis.IDENTITY
	
	var axis = rvec.normalized()
	return Basis(axis, angle)

# Converted from Python _get_centroid function
func get_centroid(detections: Array) -> Vector3:
	var transformed_positions: Array = []
	
	for detection in detections:
		var id = detection["id"]
		var rvec = detection["rvec"]
		var tvec = detection["tvec"]
		
		if id in MARKER_OFFSETS:
			# Convert rotation vector to matrix (similar to cv2.Rodrigues)
			var rotation_matrix = _rodrigues_to_matrix(rvec)
			var offset = MARKER_OFFSETS[id]
			
			# Apply transformation: R * offset + t
			var transformed = rotation_matrix * offset + tvec
			transformed_positions.append(transformed)
	
	if transformed_positions.size() == 0:
		return Vector3.ZERO
	
	# Calculate mean (centroid)
	var centroid = Vector3.ZERO
	for pos in transformed_positions:
		centroid += pos
	
	return centroid / transformed_positions.size()

# Converted from Python _get_local_coordinates function
func get_local_coordinates(first_detection: Dictionary, centroid: Vector3) -> Vector3:
	var first_id = first_detection["id"]
	var first_rvec = first_detection["rvec"]
	var first_tvec = first_detection["tvec"]
	
	# Get the rotation matrix for the first marker
	var rotation_matrix = _rodrigues_to_matrix(first_rvec)
	var offset = MARKER_OFFSETS[first_id] if first_id in MARKER_OFFSETS else Vector3.ZERO
	
	# Calculate local camera position
	var local_camera_t = rotation_matrix * offset + first_tvec
	
	# Transform to local coordinates using inverse rotation
	var local_coordinates = rotation_matrix.transposed() * (local_camera_t - centroid)
	
	return local_coordinates

# Process detections from C++ extension
func process_detections(detections: Array) -> Vector3:
	if detections.size() == 0:
		return Vector3.ZERO
	
	# Store first frame reference
	if first_frame and detections.size() > 0:
		var first_detection = detections[0]
		first_ids = [first_detection["id"]]
		first_rvecs = [first_detection["rvec"]]
		first_tvecs = [first_detection["tvec"]]
		first_frame = false
	
	# Calculate centroid from all detected markers
	var centroid = get_centroid(detections)
	
	# Get local coordinates using first frame reference
	var local_coordinates = Vector3.ZERO
	if first_ids.size() > 0:
		var first_detection = {
			"id": first_ids[0],
			"rvec": first_rvecs[0],
			"tvec": first_tvecs[0]
		}
		local_coordinates = get_local_coordinates(first_detection, centroid)
	
	# Apply EMA filter for smoothing
	local_coordinates = filter.update(local_coordinates)
	
	# Record data if enabled
	if record and csv_file:
		_write_csv_data(local_coordinates)
	
	return local_coordinates

# Data recording functions
func start_recording(patient_id: String):
	current_patient_id = patient_id
	_setup_session_directory()
	record = true

func stop_recording():
	record = false
	if csv_file:
		csv_file.close()
		csv_file = null

func _setup_session_directory():
	var date_str = _get_date_string()
	session_path = "user://data/" + current_patient_id + "/Session-" + date_str + "/MovementData"
	
	# Create directory if it doesn't exist
	if not DirAccess.dir_exists_absolute(session_path):
		DirAccess.open("user://").make_dir_recursive(session_path)
	
	# Create CSV file with timestamp
	var timestamp = Time.get_datetime_string_from_system().replace(":", "-")
	var csv_filename = session_path + "/" + timestamp + "_data.csv"
	
	csv_file = FileAccess.open(csv_filename, FileAccess.WRITE)
	if csv_file:
		csv_file.store_line("Time,X,Y,Z")  # CSV header

func _write_csv_data(coordinates: Vector3):
	if csv_file:
		var timestamp = Time.get_datetime_string_from_system()
		var line = "%s,%.6f,%.6f,%.6f" % [timestamp, coordinates.x, coordinates.y, coordinates.z]
		csv_file.store_line(line)
		csv_file.flush()  # Ensure data is written immediately

func _get_date_string() -> String:
	var date = Time.get_date_dict_from_system()
	return "%04d-%02d-%02d" % [date.year, date.month, date.day]

# Reset tracking (similar to Python RESET message)
func reset_tracking():
	first_frame = true
	first_ids.clear()
	first_rvecs.clear()
	first_tvecs.clear()
	filter = ExponentialMovingAverageFilter3D.new(ALPHA)  # Reset filter
