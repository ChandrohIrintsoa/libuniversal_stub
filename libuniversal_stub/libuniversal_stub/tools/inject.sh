#!/system/bin/sh
# UniversalStub Injection Script v5.0.0-RE
# Usage: ./inject.sh <package_name> [options]

LIB_PATH="/data/local/tmp/libuniversalstub.so"
LOG_TAG="UniversalStub"

# Colors for output (if terminal supports it)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    echo "UniversalStub Injection Tool v5.0.0-RE"
    echo "======================================"
    echo ""
    echo "Usage: $0 <package_name> [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -i, --install       Install library to device"
    echo "  -r, --remove        Remove library from device"
    echo "  -s, --status        Check injection status"
    echo "  -l, --logs          Show real-time logs"
    echo "  -k, --kill          Kill target app after injection"
    echo "  -f, --force         Force injection (kill if running)"
    echo "  -d, --debug         Enable debug mode"
    echo "  -c, --check         Check environment"
    echo "  -a, --all           Inject into all apps"
    echo "  -v, --version       Show version"
    echo ""
    echo "Examples:"
    echo "  $0 com.example.app"
    echo "  $0 com.example.app -f -k"
    echo "  $0 -i"
    echo "  $0 com.example.app -l"
    echo ""
}

check_root() {
    if [ "$(id -u)" != "0" ]; then
        log_warn "Not running as root. Some features may not work."
        return 1
    fi
    return 0
}

check_lib() {
    if [ ! -f "$LIB_PATH" ]; then
        log_error "Library not found: $LIB_PATH"
        log_info "Run with -i to install"
        return 1
    fi

    # Check architecture compatibility
    local lib_arch=$(file "$LIB_PATH" 2>/dev/null | grep -oE 'ARM aarch64|ARM|x86-64|Intel 80386')
    local device_arch=$(getprop ro.product.cpu.abi 2>/dev/null)

    log_info "Library arch: $lib_arch"
    log_info "Device arch: $device_arch"

    case "$device_arch" in
        arm64-v8a)
            if ! echo "$lib_arch" | grep -q "aarch64"; then
                log_warn "Architecture mismatch! Library may not work."
            fi
            ;;
        armeabi-v7a)
            if ! echo "$lib_arch" | grep -q "ARM"; then
                log_warn "Architecture mismatch! Library may not work."
            fi
            ;;
        x86_64)
            if ! echo "$lib_arch" | grep -q "x86-64"; then
                log_warn "Architecture mismatch! Library may not work."
            fi
            ;;
        x86)
            if ! echo "$lib_arch" | grep -q "80386"; then
                log_warn "Architecture mismatch! Library may not work."
            fi
            ;;
    esac

    # Check library file integrity
    local lib_size=$(stat -c%s "$LIB_PATH" 2>/dev/null || echo "0")
    if [ "$lib_size" -lt 1024 ]; then
        log_error "Library file too small ($lib_size bytes). May be corrupted."
        return 1
    fi

    log_success "Library check passed (${lib_size} bytes)"
    return 0
}

install_lib() {
    log_info "Installing UniversalStub v5.0.0-RE..."

    # Create directory
    mkdir -p /data/local/tmp/universal_stub

    # Push library (assumes it's in current directory or adb push was done)
    if [ -f "libuniversalstub.so" ]; then
        cp libuniversalstub.so "$LIB_PATH"
        chmod 755 "$LIB_PATH"
        log_success "Library installed to $LIB_PATH"
    else
        log_error "libuniversalstub.so not found in current directory"
        log_info "Please push it manually: adb push libuniversalstub.so /data/local/tmp/"
        return 1
    fi

    # Create config directory
    mkdir -p /data/local/tmp/universal_stub
    chmod 755 /data/local/tmp/universal_stub

    log_success "Installation complete"
    return 0
}

remove_lib() {
    log_info "Removing UniversalStub..."

    rm -f "$LIB_PATH"
    rm -rf /data/local/tmp/universal_stub

    # Remove wrap properties
    for prop in $(getprop 2>/dev/null | grep "wrap\." | cut -d: -f1 | tr -d '[]'); do
        resetprop "$prop" "" 2>/dev/null || setprop "$prop" "" 2>/dev/null
        log_info "Removed property: $prop"
    done

    log_success "Removal complete"
    return 0
}

inject_app() {
    local package="$1"
    local force="$2"
    local kill_app="$3"

    log_info "Injecting into $package..."

    # Check if app is running
    local pid=$(pidof "$package" 2>/dev/null)

    if [ -n "$pid" ]; then
        log_info "App is running (PID: $pid)"

        if [ "$force" = "1" ] || [ "$kill_app" = "1" ]; then
            log_info "Killing app..."
            am force-stop "$package"
            sleep 1
        else
            log_warn "App is running. Use -f or -k to restart."
            return 1
        fi
    fi

    # Set wrap property
    local wrap_prop="wrap.$package"

    if check_root 2>/dev/null; then
        resetprop "$wrap_prop" "LD_PRELOAD=$LIB_PATH" 2>/dev/null || \
            setprop "$wrap_prop" "LD_PRELOAD=$LIB_PATH"
    else
        setprop "$wrap_prop" "LD_PRELOAD=$LIB_PATH"
    fi

    log_success "Injection configured for $package"
    log_info "Start the app to apply injection"

    # Optionally start the app
    if [ "$kill_app" = "1" ]; then
        log_info "Starting app..."
        am start -n "$package/.MainActivity" 2>/dev/null || \
            monkey -p "$package" -c android.intent.category.LAUNCHER 1 2>/dev/null
    fi

    return 0
}

inject_all() {
    log_info "Injecting into all debuggable apps..."

    # Get list of installed packages
    pm list packages 2>/dev/null | cut -d: -f2 | while read package; do
        # Skip system apps (optional)
        if [ "${package#com.android}" != "$package" ]; then
            continue
        fi

        log_info "Configuring $package..."
        setprop "wrap.$package" "LD_PRELOAD=$LIB_PATH" 2>/dev/null
    done

    log_success "All apps configured"
}

check_status() {
    log_info "Checking injection status..."

    # Check library
    if [ -f "$LIB_PATH" ]; then
        log_success "Library installed: $LIB_PATH"
        ls -lh "$LIB_PATH" 2>/dev/null
    else
        log_error "Library not installed"
    fi

    # Check wrap properties
    log_info "Active wrap properties:"
    getprop 2>/dev/null | grep "wrap\." | while read line; do
        echo "  $line"
    done

    # Check running processes with library
    log_info "Processes with UniversalStub:"
    for pid in $(ls /proc 2>/dev/null | grep -E '^[0-9]+$'); do
        if [ -f "/proc/$pid/maps" ]; then
            if grep -q "libuniversalstub" "/proc/$pid/maps" 2>/dev/null; then
                local cmdline=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ')
                echo "  PID $pid: $cmdline"
            fi
        fi
    done

    # Check logs
    log_info "Recent UniversalStub logs:"
    logcat -d -s UniversalStub:D 2>/dev/null | tail -20
}

show_logs() {
    log_info "Showing real-time logs (Ctrl+C to exit)..."
    logcat -s UniversalStub:D *:S 2>/dev/null
}

check_environment() {
    log_info "Checking environment..."

    # Android version
    local android_ver=$(getprop ro.build.version.release 2>/dev/null)
    local sdk=$(getprop ro.build.version.sdk 2>/dev/null)
    log_info "Android: $android_ver (API $sdk)"

    # Architecture
    local arch=$(getprop ro.product.cpu.abi 2>/dev/null)
    local arch2=$(getprop ro.product.cpu.abi2 2>/dev/null)
    log_info "Architecture: $arch $arch2"

    # SELinux
    local selinux=$(getenforce 2>/dev/null || echo "unknown")
    log_info "SELinux: $selinux"

    # Root status
    if [ -f /system/bin/su ] || [ -f /system/xbin/su ] || [ -f /su/bin/su ]; then
        log_warn "Root detected"
    else
        log_info "No root detected"
    fi

    # Magisk
    if [ -d /data/adb/magisk ]; then
        log_warn "Magisk detected"
    fi

    # KernelSU
    if [ -d /data/adb/ksu ] || [ -f /data/adb/ksud ]; then
        log_warn "KernelSU detected"
    fi

    # APatch
    if [ -d /data/adb/apatch ]; then
        log_warn "APatch detected"
    fi

    # Xposed
    if [ -d /data/data/de.robv.android.xposed.installer ]; then
        log_warn "Xposed detected"
    fi

    # Memory
    local mem_total=$(cat /proc/meminfo 2>/dev/null | grep MemTotal | awk '{print $2}')
    local mem_free=$(cat /proc/meminfo 2>/dev/null | grep MemFree | awk '{print $2}')
    log_info "Memory: ${mem_free}K / ${mem_total}K free"

    # Storage
    local storage=$(df /data 2>/dev/null | tail -1 | awk '{print $4}')
    log_info "Storage: ${storage}K available"
}

# ==================== MAIN ====================

main() {
    local package=""
    local install=0
    local remove=0
    local status=0
    local logs=0
    local kill_app=0
    local force=0
    local debug=0
    local check=0
    local all=0

    # Parse arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            -h|--help)
                show_help
                exit 0
                ;;
            -i|--install)
                install=1
                ;;
            -r|--remove)
                remove=1
                ;;
            -s|--status)
                status=1
                ;;
            -l|--logs)
                logs=1
                ;;
            -k|--kill)
                kill_app=1
                ;;
            -f|--force)
                force=1
                ;;
            -d|--debug)
                debug=1
                ;;
            -c|--check)
                check=1
                ;;
            -a|--all)
                all=1
                ;;
            -v|--version)
                echo "UniversalStub v5.0.0-RE"
                exit 0
                ;;
            -*)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
            *)
                package="$1"
                ;;
        esac
        shift
    done

    # Execute commands
    if [ "$install" = "1" ]; then
        install_lib
    fi

    if [ "$remove" = "1" ]; then
        remove_lib
    fi

    if [ "$check" = "1" ]; then
        check_environment
    fi

    if [ "$status" = "1" ]; then
        check_status
    fi

    if [ "$logs" = "1" ]; then
        show_logs
    fi

    if [ "$all" = "1" ]; then
        inject_all
    fi

    if [ -n "$package" ]; then
        check_lib
        inject_app "$package" "$force" "$kill_app"
    fi

    # Default: show help if no action
    if [ "$install" = "0" ] && [ "$remove" = "0" ] && [ "$status" = "0" ] &&
       [ "$logs" = "0" ] && [ "$check" = "0" ] && [ "$all" = "0" ] && [ -z "$package" ]; then
        show_help
    fi
}

main "$@"
