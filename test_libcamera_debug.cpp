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
    bool hflip;
    double marker_size;
    int frame_count;

public:
    AprilTagDetector(bool enable_hflip = true, double marker_sz = 0.05) 
        : is_initialized(false), hflip(enable_hflip), marker_size(marker_sz), frame_count(0) {
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

    void process_frame(const uint8_t* data, size_t size, int width, int height, int sequence) {
        if (!is_initialized) return;

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
            std::cerr << "Unsupported frame format, size: " << size 
                      << " expected: " << (width * height) << " or " << (width * height * 2) << std::endl;
            return;
        }

        if (frame.empty()) {
            std::cerr << "Failed to create OpenCV Mat from frame data" << std::endl;
            return;
        }

        // Create both flipped and non-flipped versions for comparison
        cv::Mat frame_normal = frame.clone();
        cv::Mat frame_flipped;
        cv::flip(frame, frame_flipped, 1);

        // Save frames for debugging (every 10th frame)
        if (sequence % 10 == 0) {
            cv::imwrite("debug_frame_" + std::to_string(sequence) + "_normal.jpg", frame_normal);
            cv::imwrite("debug_frame_" + std::to_string(sequence) + "_flipped.jpg", frame_flipped);
            std::cout << "Saved debug frames for sequence " << sequence << std::endl;
        }

        // Test both orientations
        bool found_normal = test_apriltag_detection(frame_normal, sequence, "normal");
        bool found_flipped = test_apriltag_detection(frame_flipped, sequence, "flipped");

        if (!found_normal && !found_flipped) {
            std::cout << "Frame " << sequence << " (" << width << "x" << height << "): No markers detected (tried both orientations)" << std::endl;
        }
    }

private:
    bool test_apriltag_detection(const cv::Mat& frame, int sequence, const std::string& orientation) {
        // Detect AprilTags
        std::vector<std::vector<cv::Point2f>> corners;
        std::vector<int> ids;
        
        detector.detectMarkers(frame, corners, ids);
        
        if (ids.size() > 0) {
            std::vector<cv::Vec3d> rvecs, tvecs;
            cv::aruco::estimatePoseSingleMarkers(corners, marker_size, camera_matrix, dist_coeffs, rvecs, tvecs);
            
            std::cout << "*** MARKERS FOUND *** Frame " << sequence << " (" << orientation << "): Detected " << ids.size() << " markers" << std::endl;
            for (size_t i = 0; i < rvecs.size(); i++) {
                std::cout << "  Marker ID: " << ids[i] << std::endl;
                std::cout << "    rvec: [" << rvecs[i][0] << ", " << rvecs[i][1] << ", " << rvecs[i][2] << "]" << std::endl;
                std::cout << "    tvec: [" << tvecs[i][0] << ", " << tvecs[i][1] << ", " << tvecs[i][2] << "]" << std::endl;
            }

            // Save marked frame
            cv::Mat marked_frame = frame.clone();
            if (marked_frame.channels() == 1) {
                cv::cvtColor(marked_frame, marked_frame, cv::COLOR_GRAY2BGR);
            }
            cv::aruco::drawDetectedMarkers(marked_frame, corners, ids);
            for (size_t i = 0; i < rvecs.size(); i++) {
                cv::drawFrameAxes(marked_frame, camera_matrix, dist_coeffs, rvecs[i], tvecs[i], 0.02);
            }
            cv::imwrite("detected_frame_" + std::to_string(sequence) + "_" + orientation + ".jpg", marked_frame);
            
            return true;
        }
        return false;
    }
};

static std::shared_ptr<Camera> camera;
static AprilTagDetector* detector = nullptr;
static int frame_count = 0;
static const int max_frames = 30;

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
            if (memory == MAP_FAILED) {
                std::cerr << "Failed to mmap buffer" << std::endl;
            } else {
                // Get stream configuration for width/height
                const Stream *stream = bufferPair.first;
                const StreamConfiguration &streamConfig = stream->configuration();
                
                detector->process_frame(
                    static_cast<const uint8_t*>(memory),
                    plane.bytesused,
                    streamConfig.size.width,
                    streamConfig.size.height,
                    metadata.sequence
                );
                
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
    std::cout << "AprilTag 36h11 Debug - Testing both flipped and normal orientations" << std::endl;
    std::cout << "This will save debug images to see what the camera captures" << std::endl;

    // Initialize AprilTag detector
    detector = new AprilTagDetector(true, 0.05); // 5cm markers
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
        std::cout << "No cameras were identified on the system." << std::endl;
        cm->stop();
        delete detector;
        return EXIT_FAILURE;
    }

    std::string cameraId = cameras[0]->id();
    std::cout << "Using camera: " << cameraId << std::endl;
    
    camera = cm->get(cameraId);
    camera->acquire();

    // Configure camera
    std::unique_ptr<CameraConfiguration> config = camera->generateConfiguration({ StreamRole::Viewfinder });
    StreamConfiguration &streamConfig = config->at(0);

    // Set camera configuration for OV9281 - use proven working resolution
    streamConfig.size.width = 640;
    streamConfig.size.height = 400;
    streamConfig.pixelFormat = formats::R8; // Use 8-bit monochrome

    CameraConfiguration::Status validation = config->validate();
    if (validation == CameraConfiguration::Invalid) {
        std::cout << "Invalid camera configuration" << std::endl;
        camera->release();
        cm->stop();
        delete detector;
        return EXIT_FAILURE;
    }

    if (validation == CameraConfiguration::Adjusted) {
        std::cout << "Camera configuration adjusted" << std::endl;
    }

    std::cout << "Final configuration: " << streamConfig.size.width << "x" << streamConfig.size.height 
              << " " << streamConfig.pixelFormat.toString() << std::endl;

    camera->configure(config.get());

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

        size_t allocated = allocator->buffers(cfg.stream()).size();
        std::cout << "Allocated " << allocated << " buffers for stream" << std::endl;
    }

    // Create requests
    std::vector<std::unique_ptr<Request>> requests;
    Stream *stream = streamConfig.stream();
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
    
    for (unsigned int i = 0; i < buffers.size(); ++i) {
        std::unique_ptr<Request> request = camera->createRequest();
        if (!request) {
            std::cerr << "Can't create request" << std::endl;
            delete allocator;
            camera->release();
            cm->stop();
            delete detector;
            return EXIT_FAILURE;
        }

        const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
        int ret = request->addBuffer(stream, buffer.get());
        if (ret < 0) {
            std::cerr << "Can't set buffer for request" << std::endl;
            delete allocator;
            camera->release();
            cm->stop();
            delete detector;
            return EXIT_FAILURE;
        }

        requests.push_back(std::move(request));
    }

    // Connect signal and start camera
    camera->requestCompleted.connect(requestComplete);

    camera->start();
    for (std::unique_ptr<Request> &request : requests) {
        camera->queueRequest(request.get());
    }

    // Run for a while
    std::cout << "Capturing " << max_frames << " frames for AprilTag detection..." << std::endl;
    std::cout << "Debug images will be saved to current directory" << std::endl;
    
    while (frame_count < max_frames) {
        std::this_thread::sleep_for(100ms);
    }

    // Proper cleanup sequence
    std::cout << "Stopping camera..." << std::endl;
    camera->stop();
    
    std::cout << "Disconnecting signals..." << std::endl;
    camera->requestCompleted.disconnect();
    
    std::cout << "Releasing camera..." << std::endl;
    camera->release();
    camera.reset();
    
    std::cout << "Cleaning up allocator..." << std::endl;
    delete allocator;
    
    std::cout << "Stopping camera manager..." << std::endl;
    cm->stop();
    
    delete detector;

    std::cout << "Debug test completed! Check the saved debug images." << std::endl;
    return 0;
}