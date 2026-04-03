# BloodStrike CPython Integration Guide

## Overview

BloodStrike uses **CPython integration** - meaning the game has Python embedded directly into its engine. This is a powerful attack vector for cheat development because you can inject and execute Python code directly within the game's context.

---

## How BloodStrike's CPython Integration Works

BloodStrike's game logic is partially written in Python, which runs inside a CPython interpreter embedded in the game's executable. This means:

1. **Game code runs as Python scripts** - Much of the gameplay logic is in `.py` files
2. **Direct memory access** - Python scripts can access game objects directly
3. **No need for traditional memory scanning** - You can call game functions directly from Python

### Key Python Modules in BloodStrike

```
gclient/
├── framework/
│   ├── entities/
│   │   └── space.py         # Contains Space class with entities
│   └── util/
│       └── story_tick.py    # Game loop hook mechanism
├── gameplay/
│   └── logic_base/
│       └── entities/
│           └── combat_avatar.py  # Player/Enemy entity class
└── cconst.py                # Contains various game constants
```

---

## Important Classes & Modules

### Space Class (Main Game World)
```python
from gclient.framework.entities.space import Space

# Access the singleton instance
space = Space._instance

# Important properties:
space.owner         # Local player entity
space.entities      # Dict of all entities {entity_id: entity}
space.camera        # Main camera
space.placer        # Controls view angles/rotation
space.engine_camera # World-to-screen conversion
```

### CombatAvatar (Player/Enemy Entity)
```python
from gclient.gameplay.logic_base.entities.combat_avatar import CombatAvatar

# Entity properties:
entity.id           # Entity ID
entity.pos          # Position Vector3 (x, y, z)
entity.hp           # Current health
entity.max_hp       # Maximum health
entity.faction      # Team ID
entity.is_allied    # Boolean - is teammate
entity.is_alive     # Boolean - is alive
entity.is_dying_state  # Boolean - knocked down
entity.model        # Visual model (for bone positions)
```

### StoryTick (Game Loop Hook)
```python
from gclient.framework.util.story_tick import StoryTick

# Add your function to run every frame
def my_cheat_func(args):
    # Your cheat code here
    pass

StoryTick._instance.Add(my_cheat_func, 0)  # 0 = run every frame
```

---

## Key Functions for Cheating

### Bone Positions (for Aimbot/ESP)
```python
# Get bone world position
head_pos = entity.model.GetBoneWorldPosition('biped Head')
chest_pos = entity.model.GetBoneWorldPosition('biped Spine1')
```

**Available Bone Names:**
- `biped Head` - Head
- `biped Spine1` - Chest/Upper Torso
- `biped Spine` - Body/Stomach
- `biped LeftArm` - Left Arm
- `biped RightArm` - Right Arm
- `biped LeftLeg` - Left Leg
- `biped RightLeg` - Right Leg
- `biped Neck` - Neck

### World to Screen (for ESP)
```python
# Convert world position to screen coordinates
screen_pos = Space._instance.camera.engine_camera.GetScreenPointFromWorldPoint(world_pos)

# Returns (x, y, z) where z > 0 means on screen
if screen_pos[2] > 0:
    # Entity is visible on screen
    draw_x = screen_pos[0]
    draw_y = screen_pos[1]
```

### Aimbot (Camera Control)
```python
import math

def aimbot_tick(args):
    me = Space._instance.owner
    if not (me and me.is_alive):
        return
    
    cam = Space._instance.camera
    best_target = None
    min_dist = 9999
    
    for eid, ent in Space._instance.entities.items():
        if ent == me or not ent.is_alive or ent.is_allied:
            continue
        
        head_pos = ent.model.GetBoneWorldPosition('biped Head')
        screen_pos = cam.engine_camera.GetScreenPointFromWorldPoint(head_pos)
        
        if screen_pos[2] <= 0:
            continue
        
        # Distance to screen center
        cx, cy = 1920/2, 1080/2  # Your resolution
        dist = math.sqrt((screen_pos[0]-cx)**2 + (screen_pos[1]-cy)**2)
        
        if dist < min_dist and dist < 200:  # FOV check
            min_dist = dist
            best_target = ent
    
    if best_target:
        # Calculate angles and aim
        head = best_target.model.GetBoneWorldPosition('biped Head')
        # Calculate yaw/pitch to look at target
        # Then call: Space._instance.camera.placer.Rotate(yaw, pitch)

StoryTick._instance.Add(aimbot_tick, 0)
```

### Chams/ESP Glow
```python
# Apply X-ray highlight effect on entity
entity.model.UseTechHighLightXray((1, 0, 0), (0, 0, 0), (1, 0, 0))
# Parameters: (color1, color2, color3) as RGB tuples
```

---

## Skin Changer via Python

Based on community research, skin IDs can be discovered by:

1. **Scanning game files** - Look in `gclient/cconst.py` for skin constants
2. **Memory swapping** - Change skin ID in weapon customization screen
3. **Function hooks** - Hook `GetAllGunSkinItemList()` function

### Example Skin Changer (Concept)
```python
def apply_skin(weapon_id, skin_id):
    # This requires reverse engineering the actual skin application function
    # The skin IDs follow a pattern: weapon_prefix + skin_number
    # Example: AK47 skins start with 101, M4A1 skins start with 111
    pass
```

---

## Python Code Injection Methods

### Method 1: DLL Injector (Windows)
Download the BloodStrike Python Injector from UnknownCheats:
- https://www.unknowncheats.me/forum/downloads.php?do=file&id=52201

This DLL allows you to inject `.py` files into the game.

### Method 2: Memory Injection (Linux)
For Linux/Proton, you can:
1. Use `ptrace` to inject code
2. Use `LD_PRELOAD` to load a shared library
3. Hook the Python interpreter directly

### Method 3: File Replacement
Replace game Python files with modified versions:
1. Locate game's Python files
2. Modify `story_tick.py` or similar
3. Game will load your modified code

---

## Python ↔ C++ Communication

For rendering ESP with ImGui while using Python for game logic:

### Method 1: File-Based IPC
```python
# Python side - write entity data
import json

def esp_tick(args):
    data = []
    for eid, ent in Space._instance.entities.items():
        if ent.is_alive and not ent.is_allied:
            head = ent.model.GetBoneWorldPosition('biped Head')
            screen = Space._instance.camera.engine_camera.GetScreenPointFromWorldPoint(head)
            if screen[2] > 0:
                data.append({"x": screen[0], "y": screen[1], "hp": ent.hp})
    
    with open("/tmp/esp_data.json", "w") as f:
        json.dump(data, f)

StoryTick._instance.Add(esp_tick, 0.016)
```

```cpp
// C++ side - read and render
void RenderESP() {
    std::ifstream file("/tmp/esp_data.json");
    json data = json::parse(file);
    
    for (auto& enemy : data) {
        float x = enemy["x"];
        float y = enemy["y"];
        int hp = enemy["hp"];
        
        ImGui::GetBackgroundDrawList()->AddRect(
            ImVec2(x - 10, y - 10),
            ImVec2(x + 10, y + 40),
            IM_COL32(255, 0, 0, 255)
        );
    }
}
```

### Method 2: UDP Socket
```python
# Python side
import socket, json

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def esp_tick(args):
    data = []  # Collect entity data
    sock.sendto(json.dumps(data).encode(), ("127.0.0.1", 1337))

StoryTick._instance.Add(esp_tick, 0.016)
```

```cpp
// C++ side - UDP server
void StartUDPServer() {
    UdpClient listener(1337);
    while (true) {
        auto data = listener.Receive();
        auto enemies = json::parse(data);
        // Render ESP
    }
}
```

---

## Using the Offset Scanner for Skin IDs

The offset scanner in the loader can help you discover:

### 1. Weapon Entity Offsets
- Scan for patterns related to weapon structures
- Look for references to weapon IDs

### 2. Skin ID Memory Locations
- Search for known skin ID values (e.g., 11100018)
- Find the offset where current skin is stored

### 3. How to Use the Offset Scanner

1. Open the BloodStrike Loader GUI
2. Go to the "Offset Scanner" tab
3. Enter the game's PID (Process ID)
4. Click "Scan Offsets"
5. The scanner will:
   - Attach to the process
   - Scan for known UE4 patterns
   - Find weapon/skin related offsets
   - Output discovered offsets

### 4. Manual Pattern Scanning
For skin IDs specifically:

```python
# Search for skin ID in game's Python files
import os
import re

def find_skin_ids(game_dir):
    skin_ids = {}
    for root, dirs, files in os.walk(game_dir):
        for file in files:
            if file.endswith('.py'):
                with open(os.path.join(root, file), 'r') as f:
                    content = f.read()
                    # Look for skin ID patterns
                    matches = re.findall(r'SKIN_(\w+)\s*=\s*(\d+)', content)
                    for name, id in matches:
                        skin_ids[name] = int(id)
    return skin_ids
```

---

## Resources

- **BloodStrike Python SDK**: https://www.unknowncheats.me/forum/downloads.php?do=file&id=52328
- **Python Code Injector**: https://www.unknowncheats.me/forum/downloads.php?do=file&id=52201
- **UnknownCheats Forum Thread**: https://www.unknowncheats.me/forum/other-fps-games/730162-bloodstrike-python-sdk-pc.html

---

## Legal Disclaimer

This documentation is for educational purposes only. Using cheats in online games may violate Terms of Service and could result in account bans. Use at your own risk.