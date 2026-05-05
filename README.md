# Premium Loader & Injector

A professional-grade Linux injection suite, optimized for Fedora/Noabara. Featuring a stealthy ptrace-based injector and a modern Expo-powered development server for a premium UI experience.

## 🚀 Quick Start (Injection)

### 1. Requirements
- **OS**: Fedora / Nobara Linux (or any modern Linux distro)
- **Ptrace Scope**: Run `echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope` (Managed by System Doctor)
- **Game**: BloodStrike (running via Steam/Proton)

### 2. Injecting while is Running
To inject the cheat into the running process, use the CLI injector for maximum reliability:

```bash
# 1. Navigate to the build directory
cd build

# 2. Run the injector with root privileges
# -p bloodstrike: Targets the game process by name
# -l ./libbloodstrike.so: Paths to the cheat library
# -m 4: Uses the specialized Wine/Proton injection method (REQUIRED for BS)
# -v: Enables verbose logging to see exactly what's happening
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -m 4 -v
```

### 3. Using the Premium Loader GUI (Web/Local)
We've replaced the original interface with a modern Expo-powered UI.

1. **Start the Loader Server**:
   ```bash
   npm run start
   ```
2. **Access the UI**:
   - Open your browser to [http://localhost:8081](http://localhost:8081)
   - Or scan the QR code in your terminal using the **Expo Go** app on your phone.

3. **In the UI**:
   - Check the **System Doctor** to ensure your environment is ready.
   - Select the **Proton/Wine** method.
   - Click **INJECT**.

## 🛠️ Components

| Component | Description |
| :--- | :--- |
| `build/bloodstrike-injector` | Core CLI tool for low-level process injection. |
| `build/libbloodstrike.so` | The cheat engine (ESP, Aimbot, etc.). |
| `App.js` | Source for the new premium React Native loader. |
| `src/` | Full C++ source code for the injector and hooks. |

## 🛡️ Anti-Cheat & Security
- **Stealth Mode**: The injector uses advanced ELF symbol resolution to avoid common detection vectors.
- **System Doctor**: Automatically detects common configuration issues like `ptrace_scope` or SELinux blocks.
- **Ptrace Cleaning**: Automatically cleans up trace bits after successful injection.

## ⚠️ Important Notes
- Always run the injector with `sudo`.
- Ensure the game is fully loaded into the main menu before injecting.
- Press **INSERT** in-game to toggle the internal cheat menu.

---
*Developed by Antigravity - Advanced Agentic Coding*
