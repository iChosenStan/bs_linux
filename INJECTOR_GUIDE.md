# BloodStrike Linux - Stealth Injector Guide

## Overview

This guide explains how to use the stealth injector to inject `libbloodstrike.so` into BloodStrike running on Proton 9.0-4.

## Files

- **`build/bloodstrike-injector`** - Standalone injector executable
- **`build/libbloodstrike.so`** - The cheat library to inject
- **`inject.sh`** - Helper script for easy injection

## Injection Methods

### Method 1: Ptrace (Native Linux)
Best for native Linux applications. Uses ptrace to inject code into running processes.

```bash
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -m 1
```

### Method 2: Memfd (Anonymous Memory)
Creates an anonymous memory file to hide the library from disk. More stealthy.

```bash
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -m 2
```

### Method 3: LD_PRELOAD (Requires Restart)
Preloads the library before the game starts. Most reliable for Proton games.

**Steam Launch Options:**
```
LD_PRELOAD=/path/to/libbloodstrike.so %command%
```

### Method 4: Wine/Proton (Recommended for Proton Games)
Specialized method for Windows games running under Wine/Proton.

```bash
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -m 4
```

## Proton 9.0-4 Specific Instructions

### Finding the Process

BloodStrike running under Proton will have a different process structure:

```bash
# List all bloodstrike processes
./bloodstrike-injector -L -p bloodstrike

# Check process status (shows Wine/Proton info)
./bloodstrike-injector -s <pid>
```

### Recommended Injection Method for Proton

For Proton 9.0-4, **Method 4 (Wine/Proton)** is recommended:

```bash
cd /workspace/bloodstrike_linux/build
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -m 4
```

### Alternative: LD_PRELOAD (Most Reliable)

If ptrace injection fails, use LD_PRELOAD:

1. **Find your Proton prefix:**
   ```bash
   # Usually in: ~/.steam/steam/steamapps/compatdata/<appid>/
   ls ~/.steam/steam/steamapps/compatdata/
   ```

2. **Add to Steam Launch Options:**
   - Right-click BloodStrike in Steam
   - Properties → Launch Options
   - Add:
     ```
     LD_PRELOAD=/workspace/bloodstrike_linux/build/libbloodstrike.so %command%
     ```

3. **Restart the game**

### Using the Helper Script

The `inject.sh` script simplifies injection:

```bash
# Build and inject
./inject.sh --build -p bloodstrike -m 4

# List processes
./inject.sh -L -p bloodstrike

# Show Steam launch options
./inject.sh --steam

# Check process status
./inject.sh -s <pid>
```

## Anti-Cheat Considerations

### Detection Vectors

The injector includes several anti-detection features:

1. **Ptrace Trace Clearing** - Removes traces of debugger attachment
2. **Module Hiding** - Attempts to hide from `/proc/[pid]/maps`
3. **Anti-Cheat Detection** - Warns if anti-cheat modules are detected

### Checking for Anti-Cheat

```bash
# Check if anti-cheat is running
./bloodstrike-injector -s <pid>
```

### Bypassing Detection

For Proton games, anti-cheat detection is less aggressive than on Windows. However:

1. **Use LD_PRELOAD** - Most reliable method
2. **Inject before anti-cheat loads** - Start game, inject quickly
3. **Use stealth mode** - Method 2 (memfd) is harder to detect

## Troubleshooting

### "Failed to attach to thread"

**Cause:** Process has anti-debugger protection or requires higher privileges.

**Solution:**
```bash
# Run with sudo
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so
```

### "Library file not found"

**Cause:** Incorrect library path.

**Solution:**
```bash
# Use absolute path
./bloodstrike-injector -p bloodstrike -l /workspace/bloodstrike_linux/build/libbloodstrike.so
```

### "Target process not found"

**Cause:** Process name doesn't match or process isn't running.

**Solution:**
```bash
# List all processes
./bloodstrike-injector -L -p bloodstrike

# Use PID instead
./bloodstrike-injector -p <pid> -l ./libbloodstrike.so
```

### Injection works but cheat doesn't activate

**Cause:** Library loaded but initialization failed.

**Solution:**
1. Check game console/logs for errors
2. Verify Vulkan is working
3. Try LD_PRELOAD method instead

### Proton-specific issues

**Issue:** Library loads but crashes

**Solution:**
```bash
# Use Proton's LD_PRELOAD compatibility
PROTON_USE_WINED3D=1 LD_PRELOAD=./libbloodstrike.so %command%
```

**Issue:** Can't find process

**Solution:**
```bash
# Proton processes may have different names
./bloodstrike-injector -L -p "BloodStrike"
./bloodstrike-injector -L -p "bloodstrike"
./bloodstrike-injector -L -p "BSGame"
```

## Advanced Usage

### Injecting by PID

```bash
# Find PID first
pgrep -f bloodstrike

# Inject by PID
sudo ./bloodstrike-injector -p <pid> -l ./libbloodstrike.so
```

### Custom Wine Prefix

```bash
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so \
    -m 4 -w ~/.steam/steam/steamapps/compatdata/123456
```

### Verbose Mode

```bash
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -v
```

## Building from Source

```bash
cd /workspace/bloodstrike_linux

# Build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Or use the helper script
./inject.sh --build
```

## Security Notes

1. **Run as root** - Ptrace injection requires root privileges
2. **Verify source** - Only inject libraries you trust
3. **Backup saves** - Anti-cheat may ban accounts
4. **Use at own risk** - This is for educational purposes

## Proton 9.0-4 Specifics

Proton 9.0-4 uses Wine 9.0. Key differences:

- **Better DXVK/VKD3D** - Improved graphics compatibility
- **Updated Wine** - Better Windows API support
- **ESync/FSync** - Better performance

For injection, this means:
- LD_PRELOAD works reliably
- Wine-specific injection (Method 4) is well-supported
- Memory layout is similar to native Linux

## Quick Reference

```bash
# Quick injection (auto-detect method)
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so

# Proton-specific
sudo ./bloodstrike-injector -p bloodstrike -l ./libbloodstrike.so -m 4

# Using helper script
./inject.sh -p bloodstrike -m 4

# Steam launch options (add to game properties)
LD_PRELOAD=/workspace/bloodstrike_linux/build/libbloodstrike.so %command%

# List processes
./bloodstrike-injector -L -p bloodstrike

# Check status
./bloodstrike-injector -s <pid>
```

## Support

If you encounter issues:

1. Check the process is running: `pgrep -f bloodstrike`
2. Verify library exists: `ls -l libbloodstrike.so`
3. Check permissions: Run with sudo
4. Try different injection methods
5. Use LD_PRELOAD as fallback

## License

This project is for educational purposes only.