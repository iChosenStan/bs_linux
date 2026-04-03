#include "pixelbot.h"
#include "anticheat_bypass.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <X11/extensions/XTest.h>
#endif

namespace Pixelbot {

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

namespace Utils {

void rgbToHsv(uint8_t r, uint8_t g, uint8_t b, int& h, int& s, int& v) {
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;
    
    float maxVal = std::max({rf, gf, bf});
    float minVal = std::min({rf, gf, bf});
    float delta = maxVal - minVal;
    
    // Value
    v = static_cast<int>(maxVal * 255);
    
    // Saturation
    if (maxVal == 0) {
        s = 0;
        h = 0;
        return;
    }
    s = static_cast<int>((delta / maxVal) * 255);
    
    // Hue
    if (delta == 0) {
        h = 0;
    } else if (maxVal == rf) {
        h = static_cast<int>(60.0f * std::fmod((gf - bf) / delta, 6.0f));
    } else if (maxVal == gf) {
        h = static_cast<int>(60.0f * ((bf - rf) / delta + 2.0f));
    } else {
        h = static_cast<int>(60.0f * ((rf - gf) / delta + 4.0f));
    }
    
    if (h < 0) h += 360;
    h = static_cast<int>(h / 2.0f);  // OpenCV uses 0-180 for hue
}

float distance(int x1, int y1, int x2, int y2) {
    return std::sqrt(static_cast<float>((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)));
}

float angle(int x1, int y1, int x2, int y2) {
    return std::atan2(static_cast<float>(y2 - y1), static_cast<float>(x2 - x1)) * 180.0f / M_PI;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void sleepMs(int ms) {
    usleep(ms * 1000);
}

} // namespace Utils

//=============================================================================
// SCREEN CAPTURE IMPLEMENTATION
//=============================================================================

ScreenCapture::ScreenCapture() = default;

ScreenCapture::~ScreenCapture() {
#ifdef __linux__
    if (m_image) {
        XDestroyImage(m_image);
        m_image = nullptr;
    }
    if (m_display) {
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
#endif
}

bool ScreenCapture::initialize() {
#ifdef __linux__
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        printf("[Pixelbot] Failed to open X display\n");
        return false;
    }
    
    m_root = DefaultRootWindow(m_display);
    
    // Get screen dimensions
    Screen* screen = DefaultScreenOfDisplay(m_display);
    m_width = WidthOfScreen(screen);
    m_height = HeightOfScreen(screen);
    
    printf("[Pixelbot] Screen capture initialized: %dx%d\n", m_width, m_height);
    return true;
#else
    printf("[Pixelbot] Screen capture only supported on Linux with X11\n");
    return false;
#endif
}

void ScreenCapture::capture(int x, int y, int width, int height) {
#ifdef __linux__
    if (!m_display) return;
    
    if (m_image) {
        XDestroyImage(m_image);
    }
    
    m_image = XGetImage(m_display, m_root, x, y, width, height, AllPlanes, ZPixmap);
    if (m_image) {
        m_data = reinterpret_cast<uint8_t*>(m_image->data);
        m_captureX = x;
        m_captureY = y;
        m_width = width;
        m_height = height;
    }
#endif
}

void ScreenCapture::captureFullscreen() {
#ifdef __linux__
    if (!m_display) return;
    
    Screen* screen = DefaultScreenOfDisplay(m_display);
    int width = WidthOfScreen(screen);
    int height = HeightOfScreen(screen);
    
    capture(0, 0, width, height);
#endif
}

void ScreenCapture::getPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) const {
#ifdef __linux__
    if (!m_image) {
        r = g = b = 0;
        return;
    }
    
    unsigned long pixel = XGetPixel(m_image, x - m_captureX, y - m_captureY);
    
    // Extract RGB (format depends on X server, typically BGR or RGB)
    b = pixel & 0xFF;
    g = (pixel >> 8) & 0xFF;
    r = (pixel >> 16) & 0xFF;
#else
    r = g = b = 0;
#endif
}

void ScreenCapture::getPixelHSV(int x, int y, int& h, int& s, int& v) const {
    uint8_t r, g, b;
    getPixel(x, y, r, g, b);
    Utils::rgbToHsv(r, g, b, h, s, v);
}

std::vector<std::pair<int, int>> ScreenCapture::findColorPixels(const HSVRange& range) const {
    std::vector<std::pair<int, int>> pixels;
    
    if (!m_data) return pixels;
    
    // Sample every few pixels for performance
    constexpr int step = 4;
    
    for (int y = 0; y < m_height; y += step) {
        for (int x = 0; x < m_width; x += step) {
            int h, s, v;
            getPixelHSV(x + m_captureX, y + m_captureY, h, s, v);
            
            if (range.matches(h, s, v)) {
                pixels.emplace_back(x + m_captureX, y + m_captureY);
            }
        }
    }
    
    return pixels;
}

void ScreenCapture::rgbToHsv(uint8_t r, uint8_t g, uint8_t b, int& h, int& s, int& v) const {
    Utils::rgbToHsv(r, g, b, h, s, v);
}

//=============================================================================
// TARGET DETECTOR IMPLEMENTATION
//=============================================================================

TargetDetector::TargetDetector() = default;

void TargetDetector::setColorRange(const HSVRange& range) {
    m_colorRange = range;
}

void TargetDetector::setFOV(int fovDegrees, int screenWidth, int screenHeight) {
    m_fovRestricted = true;
    m_centerX = screenWidth / 2;
    m_centerY = screenHeight / 2;
    
    // Convert FOV degrees to pixels (approximate)
    float fovRad = fovDegrees * M_PI / 180.0f;
    m_fovRadius = static_cast<int>((screenHeight / 2.0f) * std::tan(fovRad / 2.0f));
}

std::vector<Target> TargetDetector::detect(const ScreenCapture& capture) {
    std::vector<std::pair<int, int>> pixels = capture.findColorPixels(m_colorRange);
    
    if (pixels.size() < static_cast<size_t>(m_minPixels)) {
        return {};
    }
    
    // Filter by FOV if enabled
    if (m_fovRestricted) {
        pixels.erase(
            std::remove_if(pixels.begin(), pixels.end(),
                [this](const std::pair<int, int>& p) {
                    return Utils::distance(p.first, p.second, m_centerX, m_centerY) > m_fovRadius;
                }),
            pixels.end()
        );
    }
    
    return clusterPixels(pixels);
}

std::vector<Target> TargetDetector::clusterPixels(
    const std::vector<std::pair<int, int>>& pixels) const {
    
    std::vector<Target> targets;
    
    if (pixels.empty()) return targets;
    
    // Simple clustering: group nearby pixels
    constexpr int clusterRadius = 100;
    
    std::vector<bool> visited(pixels.size(), false);
    
    for (size_t i = 0; i < pixels.size(); i++) {
        if (visited[i]) continue;
        
        Target target;
        target.x = pixels[i].first;
        target.y = pixels[i].second;
        target.isValid = true;
        target.pixelCount = 1;
        
        // Find all pixels in this cluster
        int minX = target.x, maxX = target.x;
        int minY = target.y, maxY = target.y;
        int sumX = target.x, sumY = target.y;
        
        visited[i] = true;
        
        for (size_t j = i + 1; j < pixels.size(); j++) {
            if (visited[j]) continue;
            
            float dist = Utils::distance(pixels[j].first, pixels[j].second, target.x, target.y);
            if (dist < clusterRadius) {
                visited[j] = true;
                sumX += pixels[j].first;
                sumY += pixels[j].second;
                target.pixelCount++;
                
                minX = std::min(minX, pixels[j].first);
                maxX = std::max(maxX, pixels[j].first);
                minY = std::min(minY, pixels[j].second);
                maxY = std::max(maxY, pixels[j].second);
            }
        }
        
        // Calculate cluster center
        target.x = sumX / target.pixelCount;
        target.y = sumY / target.pixelCount;
        target.width = maxX - minX;
        target.height = maxY - minY;
        target.confidence = static_cast<float>(target.pixelCount) / 100.0f;
        
        if (target.pixelCount >= m_minPixels) {
            targets.push_back(target);
        }
    }
    
    // Sort by pixel count (most likely target first)
    std::sort(targets.begin(), targets.end(),
        [](const Target& a, const Target& b) {
            return a.pixelCount > b.pixelCount;
        });
    
    return targets;
}

Target TargetDetector::getBestTarget(const std::vector<Target>& targets,
                                      int centerX, int centerY) const {
    if (targets.empty()) {
        return {0, 0, 0, 0, 0, 0, false};
    }
    
    // Find target closest to crosshair
    Target best = targets[0];
    float bestDist = Utils::distance(targets[0].x, targets[0].y, centerX, centerY);
    
    for (size_t i = 1; i < targets.size(); i++) {
        float dist = Utils::distance(targets[i].x, targets[i].y, centerX, centerY);
        if (dist < bestDist) {
            bestDist = dist;
            best = targets[i];
        }
    }
    
    return best;
}

//=============================================================================
// MOUSE CONTROLLER IMPLEMENTATION
//=============================================================================

MouseController::MouseController() = default;

MouseController::~MouseController() {
    shutdown();
}

bool MouseController::initialize() {
#ifdef __linux__
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        printf("[Pixelbot] Failed to open X display for mouse control\n");
        return false;
    }
    
    XTestGrabControl(m_display, True);
    printf("[Pixelbot] Mouse controller initialized (Gen 1: XTest)\n");
    return true;
#else
    printf("[Pixelbot] Mouse control only supported on Linux with X11\n");
    return false;
#endif
}

void MouseController::shutdown() {
#ifdef __linux__
    if (m_display) {
        XTestGrabControl(m_display, False);
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
#endif
    disconnectArduino();
}

void MouseController::move(int dx, int dy) {
    switch (m_gen) {
        case MouseGen::GEN_1_SOFTWARE:
        case MouseGen::GEN_2_DRIVER:
            moveSoftware(dx, dy);
            break;
        case MouseGen::GEN_3_ARDUINO:
        case MouseGen::GEN_4_HYPERV:
        case MouseGen::GEN_5_SPOOFED:
            moveArduino(dx, dy);
            break;
    }
}

void MouseController::moveTo(int x, int y) {
    int currentX, currentY;
    getPosition(currentX, currentY);
    move(x - currentX, y - currentY);
}

void MouseController::click() {
#ifdef __linux__
    if (!m_display) return;
    
    XTestFakeButtonEvent(m_display, 1, True, CurrentTime);
    XFlush(m_display);
    XTestFakeButtonEvent(m_display, 1, False, CurrentTime);
    XFlush(m_display);
#endif
}

void MouseController::rightClick() {
#ifdef __linux__
    if (!m_display) return;
    
    XTestFakeButtonEvent(m_display, 3, True, CurrentTime);
    XFlush(m_display);
    XTestFakeButtonEvent(m_display, 3, False, CurrentTime);
    XFlush(m_display);
#endif
}

bool MouseController::connectArduino(const std::string& port, int baudRate) {
    m_arduinoFd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (m_arduinoFd < 0) {
        printf("[Pixelbot] Failed to open Arduino port %s\n", port.c_str());
        return false;
    }
    
    // Configure serial port
    struct termios options;
    tcgetattr(m_arduinoFd, &options);
    
    cfsetispeed(&options, baudRate);
    cfsetospeed(&options, baudRate);
    
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    tcsetattr(m_arduinoFd, TCSANOW, &options);
    fcntl(m_arduinoFd, F_SETFL, 0);
    
    printf("[Pixelbot] Arduino connected on %s at %d baud\n", port.c_str(), baudRate);
    return true;
}

void MouseController::disconnectArduino() {
    if (m_arduinoFd >= 0) {
        close(m_arduinoFd);
        m_arduinoFd = -1;
    }
}

bool MouseController::sendArduinoMove(int dx, int dy) {
    if (m_arduinoFd < 0) return false;
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "M%d,%d\n", dx, dy);
    
    ssize_t written = write(m_arduinoFd, buffer, strlen(buffer));
    return written == static_cast<ssize_t>(strlen(buffer));
}

void MouseController::getPosition(int& x, int& y) const {
#ifdef __linux__
    if (!m_display) {
        x = y = 0;
        return;
    }
    
    Window root, child;
    int rootX, rootY;
    unsigned int mask;
    
    XQueryPointer(m_display, DefaultRootWindow(m_display),
                  &root, &child, &rootX, &rootY, &x, &y, &mask);
#endif
}

void MouseController::moveSoftware(int dx, int dy) {
#ifdef __linux__
    if (!m_display) return;
    
    XTestFakeRelativeMotionEvent(m_display, dx, dy, CurrentTime);
    XFlush(m_display);
#endif
}

void MouseController::moveArduino(int dx, int dy) {
    sendArduinoMove(dx, dy);
}

//=============================================================================
// ARDUINO COMMUNICATION
//=============================================================================

ArduinoComm::ArduinoComm() = default;

ArduinoComm::~ArduinoComm() {
    disconnect();
}

bool ArduinoComm::connect(const std::string& port, int baudRate) {
    m_fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (m_fd < 0) {
        return false;
    }
    
    if (!configureSerial(m_fd, baudRate)) {
        close(m_fd);
        m_fd = -1;
        return false;
    }
    
    m_port = port;
    return true;
}

void ArduinoComm::disconnect() {
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

bool ArduinoComm::sendMove(int dx, int dy) {
    if (m_fd < 0) return false;
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "M%d,%d\n", dx, dy);
    
    return write(m_fd, buffer, strlen(buffer)) > 0;
}

bool ArduinoComm::sendClick() {
    if (m_fd < 0) return false;
    
    return write(m_fd, "C\n", 2) == 2;
}

bool ArduinoComm::configureSerial(int fd, int baudRate) {
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        return false;
    }
    
    speed_t speed;
    switch (baudRate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        default:     speed = B115200; break;
    }
    
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    return tcsetattr(fd, TCSANOW, &options) == 0;
}

//=============================================================================
// AIMBOT IMPLEMENTATION
//=============================================================================

Aimbot::Aimbot() = default;

Aimbot::~Aimbot() {
    shutdown();
}

bool Aimbot::initialize() {
    if (!m_capture.initialize()) {
        return false;
    }
    
    if (!m_mouse.initialize()) {
        return false;
    }
    
    m_screenCenterX = m_capture.getWidth() / 2;
    m_screenCenterY = m_capture.getHeight() / 2;
    
    printf("[Pixelbot] Aimbot initialized\n");
    printf("[Pixelbot] Screen center: %d, %d\n", m_screenCenterX, m_screenCenterY);
    return true;
}

void Aimbot::shutdown() {
    m_mouse.shutdown();
}

void Aimbot::setSettings(const AimbotSettings& settings) {
    m_settings = settings;
    m_detector.setColorRange(settings.targetColor);
    
    if (settings.fovCircle) {
        m_detector.setFOV(static_cast<int>(settings.fovDegrees), 
                         m_capture.getWidth(), 
                         m_capture.getHeight());
    }
}

void Aimbot::update() {
    if (!m_settings.enabled) return;
    
    // Capture screen
    m_capture.captureFullscreen();
    
    // Detect targets
    std::vector<Target> targets = m_detector.detect(m_capture);
    
    if (targets.empty()) {
        m_currentTarget.isValid = false;
        return;
    }
    
    // Get best target
    m_currentTarget = m_detector.getBestTarget(targets, m_screenCenterX, m_screenCenterY);
    
    if (!m_currentTarget.isValid) return;
    
    // Check FOV
    if (!isInFOV(m_currentTarget)) return;
    
    // Calculate aim adjustment
    int dx, dy;
    calculateAim(m_currentTarget, dx, dy);
    
    // Apply smoothing
    applySmoothing(dx, dy);
    
    // Move mouse
    m_mouse.move(dx, dy);
}

void Aimbot::calculateAim(const Target& target, int& dx, int& dy) {
    // Adjust for target bone (head vs body)
    int targetY = target.y;
    if (m_settings.targetBone == 0) {
        // Head - aim higher
        targetY -= static_cast<int>(target.height * 0.3f);
    }
    
    dx = target.x - m_screenCenterX;
    dy = targetY - m_screenCenterY;
}

void Aimbot::applySmoothing(int& dx, int& dy) {
    if (m_settings.smoothness > 1.0f) {
        dx = static_cast<int>(dx / m_settings.smoothness);
        dy = static_cast<int>(dy / m_settings.smoothness);
    }
}

bool Aimbot::isInFOV(const Target& target) const {
    if (!m_settings.fovCircle) return true;
    
    float dist = Utils::distance(target.x, target.y, m_screenCenterX, m_screenCenterY);
    float fovPixels = (m_capture.getHeight() / 2.0f) * std::tan(m_settings.fovDegrees * M_PI / 360.0f);
    
    return dist < fovPixels;
}

} // namespace Pixelbot