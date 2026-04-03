#pragma once

#include <cstdint>

namespace Input {

// Key codes (X11 compatible)
enum class Key : int {
    Unknown = 0,
    
    // Letters
    A = 38, B = 56, C = 54, D = 40, E = 26, F = 41, G = 42, H = 43,
    I = 31, J = 44, K = 45, L = 46, M = 58, N = 57, O = 32, P = 33,
    Q = 24, R = 27, S = 39, T = 28, U = 30, V = 55, W = 25, X = 53,
    Y = 29, Z = 52,
    
    // Numbers
    Num0 = 19, Num1 = 10, Num2 = 11, Num3 = 12, Num4 = 13,
    Num5 = 14, Num6 = 15, Num7 = 16, Num8 = 17, Num9 = 18,
    
    // Function keys
    F1 = 67, F2 = 68, F3 = 69, F4 = 70, F5 = 71, F6 = 72,
    F7 = 73, F8 = 74, F9 = 75, F10 = 76, F11 = 95, F12 = 96,
    
    // Special keys
    Insert = 118,
    Delete = 119,
    Home = 110,
    End = 115,
    PageUp = 112,
    PageDown = 117,
    Escape = 9,
    Tab = 23,
    CapsLock = 66,
    ShiftLeft = 50,
    ShiftRight = 62,
    ControlLeft = 37,
    ControlRight = 105,
    AltLeft = 64,
    AltRight = 108,
    Space = 65,
    Enter = 36,
    Backspace = 22,
    
    // Arrow keys
    Left = 113,
    Right = 114,
    Up = 111,
    Down = 116,
    
    // Mouse
    MouseLeft = 1,
    MouseRight = 3,
    MouseMiddle = 2
};

// Initialize input system
void Initialize();

// Update key states (call each frame)
void Update();

// Check if key is currently pressed
bool IsKeyDown(Key key);

// Check if key was just pressed this frame
bool IsKeyPressed(Key key);

// Get mouse position
void GetMousePosition(int& x, int& y);

// Check if mouse button is pressed
bool IsMouseButtonDown(int button);

} // namespace Input