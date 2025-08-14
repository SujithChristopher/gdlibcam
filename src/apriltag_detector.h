#ifndef APRILTAG_DETECTOR_H
#define APRILTAG_DETECTOR_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <libcamera/libcamera.h>
#include <memory>

namespace godot {

class AprilTagDetector : public Resource {
	GDCLASS(AprilTagDetector, Resource)

private:
	cv::Mat camera_matrix;
	cv::Mat dist_coeffs;
	cv::aruco::Dictionary aruco_dict;
	cv::aruco::DetectorParameters detector_params;
	cv::aruco::ArucoDetector detector;
	bool is_initialized;
	double marker_size;
	
	// libcamera members
	std::unique_ptr<libcamera::CameraManager> camera_manager;
	std::shared_ptr<libcamera::Camera> camera;
	std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
	std::vector<std::unique_ptr<libcamera::Request>> requests;
	bool camera_running;
	
protected:
	static void _bind_methods();

public:
	AprilTagDetector();
	~AprilTagDetector();
	
	// Static instance for callback access
	static AprilTagDetector* current_instance;

	bool load_camera_parameters(const String &json_path);
	bool initialize_camera();
	bool start_camera();
	void stop_camera();
	Array get_latest_detections();
	
	void set_camera_matrix(const Array &matrix);
	void set_distortion_coefficients(const Array &coeffs);
	void set_marker_size(double size);
	Array get_camera_matrix() const;
	Array get_distortion_coefficients() const;
	
	// Detection result structure for Godot
	struct DetectionResult {
		int marker_id;
		Vector3 rvec;
		Vector3 tvec;
		Array corners;
	};
	
	// Public access methods for callback
	void process_frame_for_detection(cv::Mat& frame, std::vector<DetectionResult>& results);
	void requeue_request(libcamera::Request* request);
};

}

#endif