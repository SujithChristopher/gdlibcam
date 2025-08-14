#ifndef APRILTAG_DETECTOR_H
#define APRILTAG_DETECTOR_H

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>

#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <libcamera/libcamera.h>
#include <memory>
#include <atomic>

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
	
	// Video feedback members
	bool video_feedback_enabled;
	cv::Mat current_frame;
	cv::Mat video_frame_resized; // Smaller frame for video feedback
	std::mutex frame_mutex;
	Ref<ImageTexture> cached_texture; // Reuse texture instead of creating new ones
	PackedByteArray cached_byte_array; // Reuse byte array for memory efficiency
	
	// Video feedback dimensions
	static constexpr int VIDEO_WIDTH = 400;
	static constexpr int VIDEO_HEIGHT = 300;
	
	// Video frame skipping for performance
	static std::atomic<int> video_frame_counter;
	static constexpr int VIDEO_FRAME_SKIP = 10; // Only process every 10th frame for video

public:
	// Frame skipping for performance (public for callback access)
	static std::atomic<int> frame_skip_counter;
	static constexpr int PROCESS_EVERY_N_FRAMES = 1; // Process every frame for faster detection updates
	
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
	Ref<ImageTexture> get_current_frame_texture();
	void set_video_feedback_enabled(bool enabled);
	bool get_video_feedback_enabled() const;
	
	void set_camera_matrix(const Array &matrix);
	void set_distortion_coefficients(const Array &coeffs);
	void set_marker_size(double size);
	Array get_camera_matrix() const;
	Array get_distortion_coefficients() const;
	
	// Helper method to adjust camera calibration for different resolution
	void adjust_camera_matrix_for_resolution(int actual_width, int actual_height, int calibration_width, int calibration_height);
	
	// Detection result structure for Godot
	struct DetectionResult {
		int marker_id;
		Vector3 rvec;
		Vector3 tvec;
		Array corners;
	};
	
	// Public access methods for callback
	void process_frame_for_detection(cv::Mat& frame, std::vector<DetectionResult>& results);
	void store_frame_for_video_feedback(cv::Mat& frame);
	void requeue_request(libcamera::Request* request);
};

}

#endif