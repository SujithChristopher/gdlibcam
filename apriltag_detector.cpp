#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <fstream>
#include <regex>
#include <sys/mman.h>
#include <unistd.h>

#include <libcamera/libcamera.h>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

using namespace libcamera;
using namespace std::chrono_literals;

class AprilTagDetector {
private:
    cv::Mat camera_matrix;
    cv::Mat dist_coeffs;
    cv::aruco::Dictionary aruco_dict;
    cv::aruco::DetectorParameters detector_params;
    cv::aruco::ArucoDetector detector;
    bool is_initialized;
    double marker_size;

public:
    AprilTagDetector(double marker_sz = 0.05) 
        : is_initialized(false), marker_size(marker_sz) {
        aruco_dict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_APRILTAG_36h11);
        detector_params = cv::aruco::DetectorParameters();
        detector = cv::aruco::ArucoDetector(aruco_dict, detector_params);
    }

    bool load_camera_parameters(const std::string& toml_path) {
        std::ifstream file(toml_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open TOML file: " << toml_path << std::endl;
            return false;
        }

        std::string line;
        std::vector<double> camera_matrix_data;
        std::vector<double> dist_coeffs_data;
        
        bool reading_camera_matrix = false;
        bool reading_dist_coeffs = false;
        
        while (std::getline(file, line)) {
            if (line.find("camera_matrix") != std::string::npos) {
                reading_camera_matrix = true;
                reading_dist_coeffs = false;
            } else if (line.find("dist_coeffs") != std::string::npos) {
                reading_camera_matrix = false;
                reading_dist_coeffs = true;
            }
            
            if (reading_camera_matrix || reading_dist_coeffs) {
                std::regex number_regex("[-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?");
                std::sregex_iterator iter(line.begin(), line.end(), number_regex);
                std::sregex_iterator end;
                
                while (iter != end) {
                    double value = std::stod(iter->str());
                    if (reading_camera_matrix) {
                        camera_matrix_data.push_back(value);
                    } else if (reading_dist_coeffs) {
                        dist_coeffs_data.push_back(value);
                    }
                    ++iter;
                }
            }
        }
        file.close();

        if (camera_matrix_data.size() != 9 || dist_coeffs_data.size() != 4) {
            std::cerr << "Invalid camera parameters size" << std::endl;
            return false;
        }

        camera_matrix = cv::Mat(3, 3, CV_64F, camera_matrix_data.data()).clone();
        dist_coeffs = cv::Mat(4, 1, CV_64F, dist_coeffs_data.data()).clone();
        
        is_initialized = true;
        std::cout << "Camera parameters loaded successfully" << std::endl;
        return true;
    }

    struct DetectionResult {
        int marker_id;
        cv::Vec3d rvec;
        cv::Vec3d tvec;
        std::vector<cv::Point2f> corners;
    };

    std::vector<DetectionResult> process_frame(const uint8_t* data, size_t size, int width, int height) {
        std::vector<DetectionResult> results;
        
        if (!is_initialized) return results;

        // Create OpenCV Mat from libcamera frame data
        cv::Mat frame;
        if (size == static_cast<size_t>(width * height)) {
            // Monochrome frame (Y8/GREY)
            frame = cv::Mat(height, width, CV_8UC1, (void*)data);
        } else if (size == static_cast<size_t>(width * height * 2)) {
            // Y16 format - convert to Y8
            cv::Mat frame16(height, width, CV_16UC1, (void*)data);
            frame16.convertTo(frame, CV_8UC1, 1.0/256.0);
        } else {
            return results;
        }

        if (frame.empty()) return results;

        // Detect AprilTags (no flip needed - normal orientation works)
        std::vector<std::vector<cv::Point2f>> corners;
        std::vector<int> ids;
        
        detector.detectMarkers(frame, corners, ids);
        
        if (ids.size() > 0) {
            std::vector<cv::Vec3d> rvecs, tvecs;
            cv::aruco::estimatePoseSingleMarkers(corners, marker_size, camera_matrix, dist_coeffs, rvecs, tvecs);
            
            for (size_t i = 0; i < rvecs.size(); i++) {
                DetectionResult result;
                result.marker_id = ids[i];
                result.rvec = rvecs[i];
                result.tvec = tvecs[i];
                result.corners = corners[i];
                results.push_back(result);
            }
        }
        
        return results;
    }
};

static std::shared_ptr<Camera> camera;
static AprilTagDetector* detector = nullptr;
static int frame_count = 0;
static int max_frames = 100; // Run longer for continuous operation

static void requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled)
        return;

    const std::map<const Stream *, FrameBuffer *> &buffers = request->buffers();

    for (auto bufferPair : buffers) {
        FrameBuffer *buffer = bufferPair.second;
        const FrameMetadata &metadata = buffer->metadata();

        if (detector && frame_count < max_frames) {
            // Get the first plane data
            const FrameMetadata::Plane &plane = metadata.planes()[0];
            
            // Map the buffer to access image data
            void *memory = mmap(NULL, plane.bytesused, PROT_READ, MAP_SHARED, buffer->planes()[0].fd.get(), 0);
            if (memory != MAP_FAILED) {
                // Get stream configuration for width/height
                const Stream *stream = bufferPair.first;
                const StreamConfiguration &streamConfig = stream->configuration();
                
                auto results = detector->process_frame(
                    static_cast<const uint8_t*>(memory),
                    plane.bytesused,
                    streamConfig.size.width,
                    streamConfig.size.height
                );
                
                // Print detection results
                if (!results.empty()) {
                    std::cout << "Frame " << metadata.sequence << ": Detected " << results.size() << " markers" << std::endl;
                    for (const auto& result : results) {
                        std::cout << "  ID " << result.marker_id 
                                  << " - tvec: [" << result.tvec[0] << ", " << result.tvec[1] << ", " << result.tvec[2] << "]"
                                  << " - rvec: [" << result.rvec[0] << ", " << result.rvec[1] << ", " << result.rvec[2] << "]" << std::endl;
                    }
                }
                
                munmap(memory, plane.bytesused);
            }
        }

        frame_count++;
        
        // Stop after processing enough frames
        if (frame_count >= max_frames) {
            return;
        }

        request->reuse(Request::ReuseBuffers);
        camera->queueRequest(request);
    }
}

int main(int argc, char* argv[])
{
    std::cout << "AprilTag 36h11 Detection - Final Production Version" << std::endl;
    std::cout << "Direct libcamera API - Zero GStreamer overhead" << std::endl;

    // Parse arguments for marker size
    double marker_size = 0.05; // 5cm default
    if (argc > 1) {
        marker_size = std::stod(argv[1]);
        std::cout << "Using marker size: " << marker_size << "m" << std::endl;
    }

    // Initialize AprilTag detector
    detector = new AprilTagDetector(marker_size);
    if (!detector->load_camera_parameters("camera_parameters.toml")) {
        std::cerr << "Failed to load camera parameters!" << std::endl;
        delete detector;
        return EXIT_FAILURE;
    }

    // Initialize libcamera
    std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
    cm->start();

    auto cameras = cm->cameras();
    if (cameras.empty()) {
        std::cout << "No cameras found" << std::endl;
        cm->stop();
        delete detector;
        return EXIT_FAILURE;
    }

    std::string cameraId = cameras[0]->id();
    std::cout << "Using camera: " << cameraId << std::endl;
    
    camera = cm->get(cameraId);
    camera->acquire();

    // Configure camera - Match calibration parameters
    std::unique_ptr<CameraConfiguration> config = camera->generateConfiguration({ StreamRole::Viewfinder });
    StreamConfiguration &streamConfig = config->at(0);

    streamConfig.size.width = 1200;   // Match camera calibration parameters
    streamConfig.size.height = 800;
    streamConfig.pixelFormat = formats::R8; // 8-bit monochrome

    CameraConfiguration::Status validation = config->validate();
    if (validation == CameraConfiguration::Invalid) {
        std::cout << "Invalid camera configuration" << std::endl;
        camera->release();
        cm->stop();
        delete detector;
        return EXIT_FAILURE;
    }

    std::cout << "Configuration: " << streamConfig.size.width << "x" << streamConfig.size.height 
              << " " << streamConfig.pixelFormat.toString() << std::endl;

    camera->configure(config.get());

    // Set camera controls - exposure
    ControlList controls;
    controls.set(controls::ExposureTime, 5000); // 5000 microseconds = 5ms
    
    // Create frame buffers
    FrameBufferAllocator *allocator = new FrameBufferAllocator(camera);
    
    for (StreamConfiguration &cfg : *config) {
        int ret = allocator->allocate(cfg.stream());
        if (ret < 0) {
            std::cerr << "Can't allocate buffers" << std::endl;
            delete allocator;
            camera->release();
            cm->stop();
            delete detector;
            return EXIT_FAILURE;
        }
    }

    // Create requests
    std::vector<std::unique_ptr<Request>> requests;
    Stream *stream = streamConfig.stream();
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
    
    for (unsigned int i = 0; i < buffers.size(); ++i) {
        std::unique_ptr<Request> request = camera->createRequest();
        if (!request) {
            delete allocator;
            camera->release();
            cm->stop();
            delete detector;
            return EXIT_FAILURE;
        }

        const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
        int ret = request->addBuffer(stream, buffer.get());
        if (ret < 0) {
            delete allocator;
            camera->release();
            cm->stop();
            delete detector;
            return EXIT_FAILURE;
        }

        // Apply exposure control to each request
        request->controls().set(controls::ExposureTime, 5000);

        requests.push_back(std::move(request));
    }

    // Connect signal and start camera
    camera->requestCompleted.connect(requestComplete);

    camera->start();
    for (std::unique_ptr<Request> &request : requests) {
        camera->queueRequest(request.get());
    }

    std::cout << "AprilTag detection started. Press Ctrl+C to stop." << std::endl;
    
    while (frame_count < max_frames) {
        std::this_thread::sleep_for(100ms);
    }

    // Cleanup
    camera->stop();
    camera->requestCompleted.disconnect();
    camera->release();
    camera.reset();
    
    delete allocator;
    cm->stop();
    delete detector;

    std::cout << "AprilTag detection completed!" << std::endl;
    return 0;
}