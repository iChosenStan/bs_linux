/*
 * Pixelbot Arduino Controller
 * 
 * Based on HITB SecConf 2023 - "Bypassing Anti-Cheats & Hacking Competitive Games"
 * by Rohan Aggarwal (@nahoragg)
 * 
 * This Arduino sketch receives movement commands from the PC and moves the mouse
 * as a second mouse (Gen 3) or through USB Host Shield with spoofed real mouse (Gen 5).
 * 
 * Generations:
 * - Gen 3: Arduino acts as second mouse
 * - Gen 4: Arduino + Hyper-V virtual mouse
 * - Gen 5: Arduino + USB Host Shield with real spoofed mouse
 * 
 * Hardware required:
 * - Arduino Leonardo (or any ATmega32U4 board)
 * - USB Host Shield (optional, for Gen 5)
 * 
 * Commands (Serial, 115200 baud):
 * - "M<dx>,<dy>\n" - Move mouse by dx, dy pixels
 * - "C\n" - Left click
 * - "R\n" - Right click
 */

#include <Mouse.h>

// Configuration
#define SERIAL_BAUD 115200
#define MAX_MOVE 127  // Maximum movement per command (HID limit)

// Pin definitions for USB Host Shield (Gen 5)
#define USB_HOST_SS 10
#define USB_HOST_INT 9

// Command buffer
char buffer[64];
int bufferIndex = 0;

// State
bool mouseAttached = false;
int totalDx = 0;
int totalDy = 0;

void setup() {
  // Initialize serial
  Serial.begin(SERIAL_BAUD);
  
  // Wait for serial connection (for debugging)
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("Pixelbot Arduino v1.0");
  Serial.println("Ready for commands...");
  
  // Initialize mouse
  Mouse.begin();
  mouseAttached = true;
  
  // LED indicator
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  // Read serial commands
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      // End of command, process it
      if (bufferIndex > 0) {
        buffer[bufferIndex] = '\0';
        processCommand(buffer);
        bufferIndex = 0;
      }
    } else {
      // Add to buffer
      if (bufferIndex < (int)(sizeof(buffer) - 1)) {
        buffer[bufferIndex++] = c;
      }
    }
  }
  
  // Blink LED to show activity
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    lastBlink = millis();
  }
}

void processCommand(const char* cmd) {
  if (strlen(cmd) == 0) return;
  
  switch (cmd[0]) {
    case 'M':
      // Move command: "M<dx>,<dy>"
      handleMoveCommand(cmd);
      break;
      
    case 'C':
      // Click command
      handleClick();
      break;
      
    case 'R':
      // Right click command
      handleRightClick();
      break;
      
    case 'S':
      // Status command
      sendStatus();
      break;
      
    default:
      Serial.print("Unknown command: ");
      Serial.println(cmd);
      break;
  }
}

void handleMoveCommand(const char* cmd) {
  // Parse "M<dx>,<dy>"
  int dx = 0, dy = 0;
  
  // Skip 'M'
  const char* p = cmd + 1;
  
  // Parse dx
  dx = atoi(p);
  
  // Find comma
  p = strchr(p, ',');
  if (p) {
    p++;  // Skip comma
    dy = atoi(p);
  }
  
  // Clamp values (HID has limits)
  dx = constrain(dx, -MAX_MOVE, MAX_MOVE);
  dy = constrain(dy, -MAX_MOVE, MAX_MOVE);
  
  // Move mouse
  if (mouseAttached) {
    Mouse.move(dx, dy, 0);
    totalDx += dx;
    totalDy += dy;
  }
  
  // Debug output
  Serial.print("Move: ");
  Serial.print(dx);
  Serial.print(", ");
  Serial.println(dy);
}

void handleClick() {
  if (mouseAttached) {
    Mouse.press(MOUSE_LEFT);
    delay(10);
    Mouse.release(MOUSE_LEFT);
  }
  Serial.println("Click");
}

void handleRightClick() {
  if (mouseAttached) {
    Mouse.press(MOUSE_RIGHT);
    delay(10);
    Mouse.release(MOUSE_RIGHT);
  }
  Serial.println("Right Click");
}

void sendStatus() {
  Serial.print("Status: ");
  Serial.print(mouseAttached ? "Mouse OK" : "No Mouse");
  Serial.print(", Total: ");
  Serial.print(totalDx);
  Serial.print(", ");
  Serial.println(totalDy);
}

/*
 * For Gen 5 (USB Host Shield with spoofed real mouse):
 * 
 * 1. Connect USB Host Shield to Arduino Leonardo
 * 2. Connect real mouse to USB Host Shield
 * 3. Modify boards.txt to spoof VID/PID:
 *    - Edit: C:\Program Files (x86)\Arduino\hardware\arduino\avr\boards.txt
 *    - Change leonardo.vid.2 and leonardo.pid.2 to match real mouse VID/PID
 *    - Use https://the-sz.com/products/usbid/index.php to find VID/PID
 * 
 * Example USB Host Shield code (requires USB_Host_Shield_2.0 library):
 * 
 * #include <usbhid.h>
 * #include <usbhub.h>
 * #include <hiduniversal.h>
 * #include <hidboot.h>
 * 
 * USB Usb;
 * USBHub Hub(&Usb);
 * HIDUniversal Hid(&Usb);
 * 
 * void setup() {
 *   if (Usb.Init() == -1) {
 *     Serial.println("USB Host Shield not found");
 *     while (1);
 *   }
 *   Mouse.begin();
 * }
 * 
 * void loop() {
 *   Usb.Task();
 *   // ... process commands and move mouse
 * }
 */

/*
 * Spoofing Arduino VID/PID (makes it appear as different device):
 * 
 * In boards.txt (Arduino installation folder):
 * 
 * leonardo.name=Arduino Leonardo
 * leonardo.vid.0=0x2341      // Vendor ID
 * leonardo.pid.0=0x0036      // Product ID
 * leonardo.vid.1=0x2341
 * leonardo.pid.1=0x8036
 * leonardo.vid.2=0x2A03      // Change these
 * leonardo.pid.2=0x0036      // Change these
 * 
 * Common mouse VID/PID to spoof:
 * - Logitech: 0x046D
 * - Razer: 0x1532
 * - Microsoft: 0x045E
 * - SteelSeries: 0x1038
 * 
 * Find more at: https://the-sz.com/products/usbid/index.php
 */