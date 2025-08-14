#include "apriltag_detector.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <sys/mman.h>
#include <unistd.h>
#include <thread>
#include <mutex>

using namespace godot;
using namespace libcamera;

// Global storage for latest detections (thread-safe approach would be better for production)
static std::vector<AprilTagDetector::DetectionResult> latest_detections;
static std::mutex detection_mutex;

// Static instance pointer
AprilTagDetector* AprilTagDetector::current_instance = nullptr;

void AprilTagDetector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_camera_parameters", "json_path"), &AprilTagDetector::load_camera_parameters);
	ClassDB::bind_method(D_METHOD("initialize_camera"), &AprilTagDetector::initialize_camera);
	ClassDB::bind_method(D_METHOD("start_camera"), &AprilTagDetector::start_camera);
	ClassDB::bind_method(D_METHOD("stop_camera"), &AprilTagDetector::stop_camera);
	ClassDB::bind_method(D_METHOD("get_latest_detections"), &AprilTagDetector::get_latest_detections);
	ClassDB::bind_method(D_METHOD("set_camera_matrix", "matrix"), &AprilTagDetector::set_camera_matrix);
	ClassDB::bind_method(D_METHOD("set_distortion_coefficients", "coeffs"), &AprilTagDetector::set_distortion_coefficients);
	ClassDB::bind_method(D_METHOD("set_marker_size", "size"), &AprilTagDetector::set_marker_size);
	ClassDB::bind_method(D_METHOD("get_camera_matrix"), &AprilTagDetector::get_camera_matrix);
	ClassDB::bind_method(D_METHOD("get_distortion_coefficients"), &AprilTagDetector::get_distortion_coefficients);
}

AprilTagDetector::AprilTagDetector() : is_initialized(false), marker_size(0.05), camera_running(false) {
	UtilityFunctions::print("AprilTagDetector constructor called");
	try {
		aruco_dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
		detector_params = cv::aruco::DetectorParameters();
		detector = cv::aruco::ArucoDetector(aruco_dict, detector_params);
		current_instance = this;  // Set static instance
		UtilityFunctions::print("AprilTagDetector created successfully");
	} catch (const std::exception& e) {
		UtilityFunctions::print("Exception in AprilTagDetector constructor: ", String(e.what()));
	}
}

AprilTagDetector::~AprilTagDetector() {
	stop_camera();
}

bool AprilTagDetector::load_camera_parameters(const String &json_path) {
	// Use Godot's built-in JSON parser
	Ref<FileAccess> file = FileAccess::open(json_path, FileAccess::READ);
	if (file.is_null()) {
		UtilityFunctions::print("Failed to open JSON file: ", json_path);
		return false;
	}

	String json_text = file->get_as_text();
	file->close();

	Ref<JSON> json = memnew(JSON);
	Error parse_error = json->parse(json_text);
	if (parse_error != OK) {
		UtilityFunctions::print("Failed to parse JSON: ", json->get_error_message());
		return false;
	}

	Dictionary data = json->get_data();
	if (!data.has("calibration")) {
		UtilityFunctions::print("JSON missing 'calibration' section");
		return false;
	}

	Dictionary calibration = data["calibration"];
	if (!calibration.has("camera_matrix") || !calibration.has("dist_coeffs")) {
		UtilityFunctions::print("JSON missing camera_matrix or dist_coeffs");
		return false;
	}

	// Parse camera matrix
	Array camera_matrix_array = calibration["camera_matrix"];
	if (camera_matrix_array.size() != 3) {
		UtilityFunctions::print("Invalid camera matrix structure");
		return false;
	}

	std::vector<double> camera_matrix_data;
	for (int i = 0; i < 3; i++) {
		Array row = camera_matrix_array[i];
		if (row.size() != 3) {
			UtilityFunctions::print("Invalid camera matrix row size");
			return false;
		}
		for (int j = 0; j < 3; j++) {
			camera_matrix_data.push_back(row[j]);
		}
	}

	// Parse distortion coefficients
	Array dist_coeffs_array = calibration["dist_coeffs"];
	if (dist_coeffs_array.size() != 4) {
		UtilityFunctions::print("Invalid distortion coefficients size");
		return false;
	}

	std::vector<double> dist_coeffs_data;
	for (int i = 0; i < 4; i++) {
		Array coeff = dist_coeffs_array[i];
		if (coeff.size() != 1) {
			UtilityFunctions::print("Invalid distortion coefficient format");
			return false;
		}
		dist_coeffs_data.push_back(coeff[0]);
	}

	camera_matrix = cv::Mat(3, 3, CV_64F, camera_matrix_data.data()).clone();
	dist_coeffs = cv::Mat(4, 1, CV_64F, dist_coeffs_data.data()).clone();
	
	is_initialized = true;
	UtilityFunctions::print("Camera parameters loaded successfully");
	return true;
}

bool AprilTagDetector::initialize_camera() {
	if (camera_running) {
		UtilityFunctions::print("Camera already running");
		return false;
	}

	// Initialize libcamera
	camera_manager = std::make_unique<CameraManager>();
	camera_manager->start();

	auto cameras = camera_manager->cameras();
	if (cameras.empty()) {
		UtilityFunctions::print("No cameras found");
		return false;
	}

	std::string cameraId = cameras[0]->id();
	camera = camera_manager->get(cameraId);
	camera->acquire();

	// Configure camera - Match calibration parameters
	std::unique_ptr<CameraConfiguration> config = camera->generateConfiguration({ StreamRole::Viewfinder });
	StreamConfiguration &streamConfig = config->at(0);

	streamConfig.size.width = 1200;   // Match camera calibration parameters
	streamConfig.size.height = 800;
	streamConfig.pixelFormat = formats::R8; // 8-bit monochrome (same as working version)

	CameraConfiguration::Status validation = config->validate();
	if (validation == CameraConfiguration::Invalid) {
		UtilityFunctions::print("Invalid camera configuration");
		return false;
	}

	camera->configure(config.get());
	
	// Debug output to match working version
	UtilityFunctions::print("Configuration: ", String::num_int64(streamConfig.size.width), "x", 
		String::num_int64(streamConfig.size.height), " ", String(streamConfig.pixelFormat.toString().c_str()));

	// Create frame buffers
	allocator = std::make_unique<FrameBufferAllocator>(camera);
	
	for (StreamConfiguration &cfg : *config) {
		int ret = allocator->allocate(cfg.stream());
		if (ret < 0) {
			UtilityFunctions::print("Can't allocate buffers");
			return false;
		}
	}

	// Create requests
	Stream *stream = streamConfig.stream();
	const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
	
	for (unsigned int i = 0; i < buffers.size(); ++i) {
		std::unique_ptr<Request> request = camera->createRequest();
		if (!request) {
			UtilityFunctions::print("Can't create request");
			return false;
		}

		const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
		int ret = request->addBuffer(stream, buffer.get());
		if (ret < 0) {
			UtilityFunctions::print("Can't set buffer for request");
			return false;
		}

		// Apply exposure control to each request
		request->controls().set(controls::ExposureTime, 5000);

		requests.push_back(std::move(request));
	}

	UtilityFunctions::print("Camera initialized successfully");
	return true;
}

// Request completion callback
static void requestComplete(Request *request) {
	if (request->status() == Request::RequestCancelled)
		return;

	const std::map<const Stream *, FrameBuffer *> &buffers = request->buffers();

	for (auto bufferPair : buffers) {
		FrameBuffer *buffer = bufferPair.second;
		const FrameMetadata &metadata = buffer->metadata();

		// Get the first plane data
		const FrameMetadata::Plane &plane = metadata.planes()[0];
		
		// Map the buffer to access image data
		void *memory = mmap(NULL, plane.bytesused, PROT_READ, MAP_SHARED, buffer->planes()[0].fd.get(), 0);
		if (memory != MAP_FAILED) {
			// Get stream configuration for width/height
			const Stream *stream = bufferPair.first;
			const StreamConfiguration &streamConfig = stream->configuration();
			
			// Process frame for AprilTag detection
			std::vector<AprilTagDetector::DetectionResult> results;
			
			// Create OpenCV Mat from libcamera frame data
			cv::Mat frame;
			size_t expected_8bit = streamConfig.size.width * streamConfig.size.height;
			size_t expected_16bit = expected_8bit * 2;
			
			if (plane.bytesused == expected_8bit) {
				// 8-bit monochrome
				frame = cv::Mat(streamConfig.size.height, streamConfig.size.width, CV_8UC1, memory);
			} else if (plane.bytesused == expected_16bit) {
				// 16-bit format - convert to 8-bit like working version does
				cv::Mat frame16(streamConfig.size.height, streamConfig.size.width, CV_16UC1, memory);
				frame16.convertTo(frame, CV_8UC1, 1.0/256.0);
			} else {
				UtilityFunctions::print("Unexpected frame size: ", String::num_int64(plane.bytesused), 
					" expected 8bit: ", String::num_int64(expected_8bit), 
					" or 16bit: ", String::num_int64(expected_16bit));
			}

			if (!frame.empty() && AprilTagDetector::current_instance) {
				AprilTagDetector* instance = AprilTagDetector::current_instance;
				
				// Process frame using the public method
				std::vector<AprilTagDetector::DetectionResult> results;
				instance->process_frame_for_detection(frame, results);
				
				// Store results
				std::lock_guard<std::mutex> lock(detection_mutex);
				latest_detections = results;
			}
			
			munmap(memory, plane.bytesused);
		}

		request->reuse(Request::ReuseBuffers);
		// Requeue the request like the working version
		if (AprilTagDetector::current_instance) {
			AprilTagDetector::current_instance->requeue_request(request);
		}
	}
}

bool AprilTagDetector::start_camera() {
	if (!camera || camera_running) {
		return false;
	}

	// Connect signal and start camera
	camera->requestCompleted.connect(requestComplete);

	camera->start();
	for (std::unique_ptr<Request> &request : requests) {
		camera->queueRequest(request.get());
	}

	camera_running = true;
	UtilityFunctions::print("Camera started");
	return true;
}

void AprilTagDetector::stop_camera() {
	if (camera_running && camera) {
		camera->stop();
		camera->requestCompleted.disconnect();
		camera_running = false;
	}

	if (camera) {
		camera->release();
		camera.reset();
	}

	requests.clear();
	allocator.reset();

	if (camera_manager) {
		camera_manager->stop();
		camera_manager.reset();
	}

	UtilityFunctions::print("Camera stopped");
}

Array AprilTagDetector::get_latest_detections() {
	Array results;
	
	std::lock_guard<std::mutex> lock(detection_mutex);
	for (const auto& detection : latest_detections) {
		Dictionary result;
		result["id"] = detection.marker_id;
		result["rvec"] = detection.rvec;
		result["tvec"] = detection.tvec;
		result["corners"] = detection.corners;
		results.append(result);
	}
	
	return results;
}

void AprilTagDetector::set_camera_matrix(const Array &matrix) {
	if (matrix.size() != 9) {
		UtilityFunctions::print("Camera matrix must have 9 elements");
		return;
	}
	
	std::vector<double> data;
	for (int i = 0; i < 9; i++) {
		data.push_back(matrix[i]);
	}
	
	camera_matrix = cv::Mat(3, 3, CV_64F, data.data()).clone();
	is_initialized = true;
}

void AprilTagDetector::set_distortion_coefficients(const Array &coeffs) {
	if (coeffs.size() != 4) {
		UtilityFunctions::print("Distortion coefficients must have 4 elements");
		return;
	}
	
	std::vector<double> data;
	for (int i = 0; i < 4; i++) {
		data.push_back(coeffs[i]);
	}
	
	dist_coeffs = cv::Mat(4, 1, CV_64F, data.data()).clone();
}

void AprilTagDetector::set_marker_size(double size) {
	marker_size = size;
}

Array AprilTagDetector::get_camera_matrix() const {
	Array result;
	if (camera_matrix.empty()) return result;
	
	for (int i = 0; i < 9; i++) {
		result.append(camera_matrix.at<double>(i / 3, i % 3));
	}
	return result;
}

Array AprilTagDetector::get_distortion_coefficients() const {
	Array result;
	if (dist_coeffs.empty()) return result;
	
	for (int i = 0; i < 4; i++) {
		result.append(dist_coeffs.at<double>(i, 0));
	}
	return result;
}

void AprilTagDetector::process_frame_for_detection(cv::Mat& frame, std::vector<DetectionResult>& results) {
	results.clear();
	
	std::vector<std::vector<cv::Point2f>> corners;
	std::vector<int> ids;
	
	// Use the instance's detector
	detector.detectMarkers(frame, corners, ids);
	
	// Debug output
	if (ids.size() > 0) {
		UtilityFunctions::print("Detected ", String::num_int64(ids.size()), " markers");
	}
	
	for (size_t i = 0; i < ids.size(); i++) {
		DetectionResult result;
		result.marker_id = ids[i];
		
		// Perform pose estimation if camera is calibrated
		if (is_initialized && !camera_matrix.empty() && !dist_coeffs.empty()) {
			std::vector<cv::Vec3d> rvecs, tvecs;
			cv::aruco::estimatePoseSingleMarkers(corners, marker_size, 
				camera_matrix, dist_coeffs, rvecs, tvecs);
			
			if (i < rvecs.size() && i < tvecs.size()) {
				result.rvec = Vector3(rvecs[i][0], rvecs[i][1], rvecs[i][2]);
				result.tvec = Vector3(tvecs[i][0], tvecs[i][1], tvecs[i][2]);
			}
		} else {
			result.rvec = Vector3(0, 0, 0);
			result.tvec = Vector3(0, 0, 0);
		}
		
		Array corner_array;
		for (const auto& corner : corners[i]) {
			Array point;
			point.append(corner.x);
			point.append(corner.y);
			corner_array.append(point);
		}
		result.corners = corner_array;
		
		results.push_back(result);
	}
}

void AprilTagDetector::requeue_request(libcamera::Request* request) {
	if (camera && camera_running) {
		camera->queueRequest(request);
	}
}