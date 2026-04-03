#pragma once

//=============================================================================
// PIXELBOT IMPLEMENTATION FOR LINUX
// Based on HITB SecConf 2023 - Hardware Bypass Techniques
// by Rohan Aggarwal (@nahoragg)
//
// Pixelbots use screen color detection to find enemies and aim at them.
// This is much harder to detect than memory-based cheats because:
// - No memory manipulation required
// - Works on any game with visible enemy outlines/colors
// - Can use external hardware (Arduino) for mouse movement
//=============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <tuple>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace Pixelbot {

//=============================================================================
// COLOR DETECTION CONFIGURATION
//=============================================================================

struct HSVRange {
    int hMin, hMax;     // Hue (0-180)
    int sMin, sMax;     // Saturation (0-255)
    int vMin, vMax;     // Value (0-255)
    
    bool matches(int h, int s, int v) const {
        return h >= hMin && h <= hMax &&
               s >= sMin && s <= sMax &&
               v >= vMin && v <= vMax;
    }
};

// Common enemy outline colors (need calibration per game)
// Red team: typically red/magenta outlines
// Blue team: typically blue/cyan outlines
namespace EnemyColors {
    // Valorant enemy outlines (red)
    constexpr HSVRange VALORANT_RED = {160, 180, 100, 255, 100, 255};
    
    // Blood Strike enemy outlines (adjust based on game)
    constexpr HSVRange BLOODSTRIKE_ENEMY = {0, 20, 100, 255, 100, 255};  // Red-ish
}

//=============================================================================
// SCREEN CAPTURE
//=============================================================================

class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();
    
    bool initialize();
    void capture(int x, int y, int width, int height);
    void captureFullscreen();
    
    // Get pixel at position
    void getPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) const;
    void getPixelHSV(int x, int y, int& h, int& s, int& v) const;
    
    // Find all pixels matching color range
    std::vector<std::pair<int, int>> findColorPixels(const HSVRange& range) const;
    
    // Get screen dimensions
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Raw pixel access
    const uint8_t* getData() const { return m_data; }
    
private:
#ifdef __linux__
    Display* m_display = nullptr;
    Window m_root;
    XImage* m_image = nullptr;
#endif
    uint8_t* m_data = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_captureX = 0;
    int m_captureY = 0;
    
    void rgbToHsv(uint8_t r, uint8_t g, uint8_t b, int& h, int& s, int& v) const;
};

//=============================================================================
// TARGET DETECTION
//=============================================================================

struct Target {
    int x, y;               // Screen coordinates
    int width, height;      // Bounding box
    float confidence;       // Detection confidence
    int pixelCount;         // Number of matching pixels
    bool isValid;
};

class TargetDetector {
public:
    TargetDetector();
    
    // Set color range for detection
    void setColorRange(const HSVRange& range);
    
    // Set minimum pixel count for valid target
    void setMinPixels(int count) { m_minPixels = count; }
    
    // Set FOV (field of view) restriction
    void setFOV(int fovDegrees, int screenWidth, int screenHeight);
    
    // Find targets in captured screen
    std::vector<Target> detect(const ScreenCapture& capture);
    
    // Get best target (closest to crosshair)
    Target getBestTarget(const std::vector<Target>& targets, 
                         int centerX, int centerY) const;
    
    // Enable/disable FOV circle
    void setFOVRestriction(bool enabled) { m_fovRestricted = enabled; }
    
private:
    HSVRange m_colorRange;
    int m_minPixels = 50;
    bool m_fovRestricted = false;
    int m_fovRadius = 200;
    int m_centerX = 0;
    int m_centerY = 0;
    
    // Cluster nearby pixels into targets
    std::vector<Target> clusterPixels(
        const std::vector<std::pair<int, int>>& pixels) const;
};

//=============================================================================
// MOUSE MOVEMENT
//=============================================================================

// Generation types from the presentation
enum class MouseGen {
    GEN_1_SOFTWARE,     // XTest, XInput extension
    GEN_2_DRIVER,       // Input drivers
    GEN_3_ARDUINO,      // Arduino as second mouse
    GEN_4_HYPERV,       // Arduino + Hyper-V
    GEN_5_SPOOFED       // Arduino + USB Host Shield
};

class MouseController {
public:
    MouseController();
    ~MouseController();
    
    bool initialize();
    void shutdown();
    
    // Move mouse relative to current position
    void move(int dx, int dy);
    
    // Move mouse to absolute position
    void moveTo(int x, int y);
    
    // Click
    void click();
    void rightClick();
    
    // Set movement generation type
    void setGeneration(MouseGen gen) { m_gen = gen; }
    
    // Arduino serial communication (Gen 3+)
    bool connectArduino(const std::string& port, int baudRate = 115200);
    void disconnectArduino();
    bool sendArduinoMove(int dx, int dy);
    
    // Get current mouse position
    void getPosition(int& x, int& y) const;
    
private:
    MouseGen m_gen = MouseGen::GEN_1_SOFTWARE;
    
#ifdef __linux__
    Display* m_display = nullptr;
#endif
    
    // Arduino file descriptor
    int m_arduinoFd = -1;
    
    // Software mouse movement (Gen 1)
    void moveSoftware(int dx, int dy);
    
    // Arduino mouse movement (Gen 3+)
    void moveArduino(int dx, int dy);
    
    // Smoothing
    void smoothMove(int targetX, int targetY, int steps);
};

//=============================================================================
// AIMBOT LOGIC
//=============================================================================

struct AimbotSettings {
    bool enabled = false;
    bool fovCircle = true;
    float fovDegrees = 90.0f;
    float smoothness = 5.0f;
    int targetBone = 0;         // 0=head, 1=body
    bool targetClosest = true;  // Target closest to crosshair
    int activationKey = 0;      // Key to hold for aimbot
    
    // Color detection
    HSVRange targetColor;
};

class Aimbot {
public:
    Aimbot();
    ~Aimbot();
    
    bool initialize();
    void shutdown();
    
    // Main update loop
    void update();
    
    // Settings
    void setSettings(const AimbotSettings& settings);
    AimbotSettings& getSettings() { return m_settings; }
    
    // Toggle
    void setEnabled(bool enabled) { m_settings.enabled = enabled; }
    bool isEnabled() const { return m_settings.enabled; }
    
private:
    AimbotSettings m_settings;
    ScreenCapture m_capture;
    TargetDetector m_detector;
    MouseController m_mouse;
    
    // Screen center (crosshair position)
    int m_screenCenterX = 0;
    int m_screenCenterY = 0;
    
    // Current target
    Target m_currentTarget;
    
    // Calculate aim angles
    void calculateAim(const Target& target, int& dx, int& dy);
    
    // Apply smoothing
    void applySmoothing(int& dx, int& dy);
    
    // Check if target is within FOV
    bool isInFOV(const Target& target) const;
};

//=============================================================================
// ARDUINO COMMUNICATION
//=============================================================================

class ArduinoComm {
public:
    ArduinoComm();
    ~ArduinoComm();
    
    // Connect to Arduino via serial
    bool connect(const std::string& port, int baudRate = 115200);
    void disconnect();
    bool isConnected() const { return m_fd >= 0; }
    
    // Send mouse movement command
    // Format: "M<dx>,<dy>\n" (e.g., "M100,-50\n")
    bool sendMove(int dx, int dy);
    
    // Send click command
    bool sendClick();
    
    // Spoof Arduino VID/PID (requires modifying boards.txt)
    // This makes the Arduino appear as a different USB device
    // https://the-sz.com/products/usbid/index.php
    
private:
    int m_fd = -1;
    std::string m_port;
    
    // Configure serial port
    bool configureSerial(int fd, int baudRate);
};

//=============================================================================
// YOLO AI DETECTION (OPTIONAL)
//=============================================================================

// YOLO-based enemy detection (more advanced than color-based)
// Requires OpenCV with DNN module
//
// Advantages:
// - Works without enemy outlines
// - Can detect enemies in any game
// - Can classify enemy vs friendly
// - Can detect multiple targets
//
// Disadvantages:
// - Slower than color-based detection
// - Requires trained model
// - Higher CPU usage
//
// Reference: https://www.unknowncheats.me/forum/general-programming-and-reversing/485725-guide-ai-aimbot.html

#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

class YOLODetector {
public:
    YOLODetector();
    
    // Load YOLO model
    bool loadModel(const std::string& modelPath, const std::string& configPath);
    
    // Detect enemies in frame
    std::vector<Target> detect(const cv::Mat& frame, float confidence = 0.5f);
    
private:
    cv::dnn::Net m_net;
    std::vector<std::string> m_classes;
};
#endif // USE_OPENCV

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

namespace Utils {
    // RGB to HSV conversion
    void rgbToHsv(uint8_t r, uint8_t g, uint8_t b, int& h, int& s, int& v);
    
    // Calculate distance between two points
    float distance(int x1, int y1, int x2, int y2);
    
    // Calculate angle between two points
    float angle(int x1, int y1, int x2, int y2);
    
    // Linear interpolation
    float lerp(float a, float b, float t);
    
    // Clamp value
    int clamp(int value, int min, int max);
    
    // Sleep in milliseconds
    void sleepMs(int ms);
}

} // namespace Pixelbot