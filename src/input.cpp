#include "input.h"

#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>

#include <cstring>
#include <cstdio>

namespace Input {

static Display* s_display = nullptr;
static bool s_keyStates[256] = {false};
static bool s_prevKeyStates[256] = {false};
static int s_mouseX = 0;
static int s_mouseY = 0;
static bool s_mouseButtons[5] = {false};

void Initialize() {
    s_display = XOpenDisplay(nullptr);
    if (!s_display) {
        fprintf(stderr, "Failed to open X display\n");
        return;
    }
    
    // Enable XInput2 for raw input
    int xiOpcode;
    int event, error;
    if (!XQueryExtension(s_display, "XInputExtension", &xiOpcode, &event, &error)) {
        fprintf(stderr, "XInput extension not available\n");
    }
    
    memset(s_keyStates, 0, sizeof(s_keyStates));
    memset(s_prevKeyStates, 0, sizeof(s_prevKeyStates));
}

void Update() {
    if (!s_display) return;
    
    // Save previous states
    memcpy(s_prevKeyStates, s_keyStates, sizeof(s_keyStates));
    
    // Query keyboard state using XQueryKeymap
    char keys[32];
    XQueryKeymap(s_display, keys);
    
    // Parse the keymap
    for (int i = 0; i < 256; i++) {
        int keycode = i;
        s_keyStates[i] = (keys[keycode / 8] & (1 << (keycode % 8))) != 0;
    }
    
    // Get mouse position
    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    
    XQueryPointer(s_display, DefaultRootWindow(s_display), 
                  &root, &child, &rootX, &rootY, 
                  &winX, &winY, &mask);
    
    s_mouseX = rootX;
    s_mouseY = rootY;
    
    // Parse mouse buttons
    s_mouseButtons[0] = (mask & Button1Mask) != 0;  // Left
    s_mouseButtons[1] = (mask & Button3Mask) != 0;  // Right
    s_mouseButtons[2] = (mask & Button2Mask) != 0;  // Middle
    s_mouseButtons[3] = (mask & Button4Mask) != 0;  // Scroll up
    s_mouseButtons[4] = (mask & Button5Mask) != 0;  // Scroll down
}

bool IsKeyDown(Key key) {
    int keycode = static_cast<int>(key);
    if (keycode < 0 || keycode >= 256) return false;
    return s_keyStates[keycode];
}

bool IsKeyPressed(Key key) {
    int keycode = static_cast<int>(key);
    if (keycode < 0 || keycode >= 256) return false;
    return s_keyStates[keycode] && !s_prevKeyStates[keycode];
}

void GetMousePosition(int& x, int& y) {
    x = s_mouseX;
    y = s_mouseY;
}

bool IsMouseButtonDown(int button) {
    if (button < 0 || button >= 5) return false;
    return s_mouseButtons[button];
}

} // namespace Input