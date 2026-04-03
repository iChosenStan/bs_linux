#!/bin/bash
#
# BloodStrike Linux Cheat - Injection Helper Script
# Supports both native Linux and Proton/Wine games
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
INJECTOR="$BUILD_DIR/bloodstrike-injector"
LIBRARY="$BUILD_DIR/libbloodstrike.so"

print_banner() {
    echo -e "${BLUE}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║              BloodStrike Linux - Injection Helper            ║"
    echo "║                   Proton/Wine Compatible                     ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --process <name>     Target process name (default: bloodstrike)"
    echo "  -i, --pid <pid>          Target process PID"
    echo "  -l, --library <path>     Library to inject (default: build/libbloodstrike.so)"
    echo "  -m, --method <1-4>       Injection method:"
    echo "                             1 = Ptrace (default for native)"
    echo "                             2 = Memfd (anonymous memory)"
    echo "                             3 = LD_PRELOAD (requires restart)"
    echo "                             4 = Wine/Proton (for Windows games)"
    echo "  -w, --wine-prefix <path> Wine/Proton prefix path"
    echo "  -L, --list               List matching processes"
    echo "  -s, --status <pid>       Show process status"
    echo "  --steam                  Show Steam launch options for LD_PRELOAD"
    echo "  --build                  Build the project before injection"
    echo "  -h, --help               Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 -p bloodstrike                    # Inject into bloodstrike process"
    echo "  $0 -p bloodstrike -m 4               # Use Proton/Wine method"
    echo "  $0 -i 12345                          # Inject by PID"
    echo "  $0 -L -p bloodstrike                 # List matching processes"
    echo "  $0 --steam                           # Show Steam launch options"
    echo ""
}

show_steam_instructions() {
    echo -e "${GREEN}=== Steam Launch Options for LD_PRELOAD ===${NC}"
    echo ""
    echo "For Proton games, add this to Steam's launch options:"
    echo "(Right-click game → Properties → Launch Options)"
    echo ""
    echo -e "${YELLOW}Method 1: Direct LD_PRELOAD${NC}"
    echo "  LD_PRELOAD=/path/to/libbloodstrike.so %command%"
    echo ""
    echo -e "${YELLOW}Method 2: With Proton debug${NC}"
    echo "  PROTON_LOG=1 LD_PRELOAD=/path/to/libbloodstrike.so %command%"
    echo ""
    echo -e "${YELLOW}Method 3: With custom Proton prefix${NC}"
    echo "  STEAM_COMPAT_DATA_PATH=/path/to/prefix LD_PRELOAD=/path/to/libbloodstrike.so %command%"
    echo ""
    echo -e "${YELLOW}Full example for BloodStrike:${NC}"
    echo "  LD_PRELOAD=$BUILD_DIR/libbloodstrike.so %command%"
    echo ""
    echo -e "${BLUE}Note:${NC} This requires restarting the game."
    echo ""
}

check_permissions() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}[!] Warning: Not running as root.${NC}"
        echo "    Some injection methods may require sudo."
        echo "    Try: sudo $0 $@"
        echo ""
    fi
}

check_prerequisites() {
    if [ ! -f "$INJECTOR" ]; then
        echo -e "${RED}[!] Injector not found at: $INJECTOR${NC}"
        echo "    Run with --build to compile the project."
        return 1
    fi
    
    if [ ! -f "$LIBRARY" ]; then
        echo -e "${RED}[!] Library not found at: $LIBRARY${NC}"
        echo "    Run with --build to compile the project."
        return 1
    fi
    
    return 0
}

build_project() {
    echo -e "${BLUE}[*] Building project...${NC}"
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[+] Build successful!${NC}"
        echo "    Injector: $INJECTOR"
        echo "    Library:  $LIBRARY"
    else
        echo -e "${RED}[-] Build failed!${NC}"
        exit 1
    fi
}

# Parse arguments
PROCESS=""
PID=""
METHOD=""
WINE_PREFIX=""
LIBRARY_PATH=""
LIST_MODE=false
STATUS_PID=""
SHOW_STEAM=false
DO_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--process)
            PROCESS="$2"
            shift 2
            ;;
        -i|--pid)
            PID="$2"
            shift 2
            ;;
        -l|--library)
            LIBRARY_PATH="$2"
            shift 2
            ;;
        -m|--method)
            METHOD="$2"
            shift 2
            ;;
        -w|--wine-prefix)
            WINE_PREFIX="$2"
            shift 2
            ;;
        -L|--list)
            LIST_MODE=true
            shift
            ;;
        -s|--status)
            STATUS_PID="$2"
            shift 2
            ;;
        --steam)
            SHOW_STEAM=true
            shift
            ;;
        --build)
            DO_BUILD=true
            shift
            ;;
        -h|--help)
            print_banner
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

print_banner

# Handle Steam instructions
if [ "$SHOW_STEAM" = true ]; then
    show_steam_instructions
    exit 0
fi

# Build if requested
if [ "$DO_BUILD" = true ]; then
    build_project
fi

# Check prerequisites
if ! check_prerequisites; then
    exit 1
fi

# Set defaults
: ${LIBRARY_PATH:="$LIBRARY"}
: ${PROCESS:="bloodstrike"}

# Handle list mode
if [ "$LIST_MODE" = true ]; then
    echo -e "${BLUE}[*] Searching for processes matching: $PROCESS${NC}"
    "$INJECTOR" -L -p "$PROCESS"
    exit 0
fi

# Handle status mode
if [ -n "$STATUS_PID" ]; then
    "$INJECTOR" -s "$STATUS_PID"
    exit 0
fi

# Check permissions for injection
check_permissions

# Build injector command
CMD="$INJECTOR"

if [ -n "$PID" ]; then
    CMD="$CMD -p $PID"
else
    CMD="$CMD -p $PROCESS"
fi

CMD="$CMD -l $LIBRARY_PATH"

if [ -n "$METHOD" ]; then
    CMD="$CMD -m $METHOD"
fi

if [ -n "$WINE_PREFIX" ]; then
    CMD="$CMD -w $WINE_PREFIX"
fi

# Run injector
echo -e "${BLUE}[*] Running injector...${NC}"
echo "    Command: $CMD"
echo ""

exec $CMD