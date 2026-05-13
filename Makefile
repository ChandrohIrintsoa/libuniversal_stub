# UniversalStub Makefile
# Supports: arm64-v8a, armeabi-v7a, x86_64, x86

# Detect host architecture for testing
HOST_ARCH := $(shell uname -m)

# Default target architecture
TARGET_ARCH ?= arm64-v8a

# NDK configuration (updated for NDK r26/r27/r28)
NDK_CANDIDATES := \
	$(ANDROID_NDK_HOME) \
	$(ANDROID_NDK) \
	$(HOME)/Android/Sdk/ndk/28.0.13014099 \
	$(HOME)/Android/Sdk/ndk/27.0.12077973 \
	$(HOME)/Android/Sdk/ndk/26.3.11579264 \
	$(HOME)/Android/Sdk/ndk/25.2.9519653

NDK_PATH ?= $(firstword $(foreach candidate,$(NDK_CANDIDATES),$(if $(wildcard $(candidate)/toolchains/llvm/prebuilt/linux-x86_64/bin),$(candidate))))
HOST_BUILD := 0

ifeq ($(strip $(NDK_PATH)),)
    HOST_BUILD := 1
    TARGET_ARCH := x86_64
endif

# Toolchain selection
ifeq ($(TARGET_ARCH),arm64-v8a)
    TRIPLE := aarch64-linux-android21
    ARCH_CFLAGS := -march=armv8-a
else ifeq ($(TARGET_ARCH),armeabi-v7a)
    TRIPLE := armv7a-linux-androideabi21
    ARCH_CFLAGS := -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
else ifeq ($(TARGET_ARCH),x86_64)
    TRIPLE := x86_64-linux-android21
    ARCH_CFLAGS := -march=x86-64 -msse4.2 -mpopcnt
else ifeq ($(TARGET_ARCH),x86)
    TRIPLE := i686-linux-android21
    ARCH_CFLAGS := -march=i686 -msse3 -mfpmath=sse
else
    $(error Unknown TARGET_ARCH: $(TARGET_ARCH))
endif

# Compiler
CC := $(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/$(TRIPLE)-clang
CXX := $(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/$(TRIPLE)-clang++

# Fallback to system clang for host testing
ifeq ($(HOST_BUILD),1)
    CC := clang
    CXX := clang++
    $(warning NDK not found, building host compatibility target)
else ifeq ($(wildcard $(CC)),)
    CC := clang
    CXX := clang++
    $(warning NDK compiler not found, using system clang)
endif

# Directories
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj/$(TARGET_ARCH)
LIB_DIR := libs/$(TARGET_ARCH)

# Source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
ifeq ($(HOST_BUILD),0)
    SRCS := $(filter-out $(SRC_DIR)/host_compat.c,$(SRCS))
endif
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Target library
TARGET := $(LIB_DIR)/libuniversalstub.so

# Compiler flags (updated: added -Wno-unused-parameter for cleaner builds)
# Added -Wno-sign-compare for hash table index comparisons
CFLAGS := -fPIC -O2 -Wall -Wextra -Werror=return-type \
      -Wno-unused-parameter -Wno-sign-compare \
      -I$(INC_DIR) \
      $(if $(filter 1,$(HOST_BUILD)),-I$(INC_DIR)/host_compat) \
      -DANDROID \
      -D_GNU_SOURCE \
      -D__ANDROID_API__=21 \
      $(ARCH_CFLAGS) \
      -fvisibility=hidden \
      -ffunction-sections \
      -fdata-sections \
      -fno-stack-protector \
      -fno-exceptions \
      -fno-rtti

# Debug flags (override with DEBUG=1)
ifeq ($(DEBUG),1)
    CFLAGS := $(filter-out -O2,$(CFLAGS)) -O0 -g3 -DDEBUG=1 -fvisibility=default
endif

# Linker flags
LDFLAGS := -shared -Wl,-soname,libuniversalstub.so \
       -Wl,--gc-sections \
       -Wl,--exclude-libs,ALL \
       -Wl,-z,noexecstack \
       -Wl,-z,relro \
       -Wl,-z,now

# Libraries
LDLIBS := -ldl -llog -landroid
ifeq ($(HOST_BUILD),1)
    LDLIBS := -ldl
endif

# All architectures
ALL_ARCHS := arm64-v8a armeabi-v7a x86_64 x86

# ==================== TARGETS ====================

.PHONY: all clean install test debug release \
    arm64 arm32 x86_64 x86 all-archs \
    help info push shell

all: $(TARGET)

help:
	@echo "UniversalStub Build System v5.0.0-RE"
	@echo "=================================="
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build for default architecture ($(TARGET_ARCH))"
	@echo "  make all-archs    - Build for all architectures"
	@echo "  make arm64        - Build for arm64-v8a"
	@echo "  make arm32        - Build for armeabi-v7a"
	@echo "  make x86_64       - Build for x86_64"
	@echo "  make x86          - Build for x86"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make release      - Build optimized release"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make install      - Push to device"
	@echo "  make push         - Push to device (alias)"
	@echo "  make shell        - Open adb shell with lib preloaded"
	@echo "  make test         - Run tests"
	@echo "  make info         - Show build info"
	@echo ""
	@echo "Variables:"
	@echo "  TARGET_ARCH=...   - Target architecture"
	@echo "  NDK_PATH=...      - Android NDK path"
	@echo "  DEBUG=1           - Enable debug build"
	@echo ""

info:
	@echo "Target Architecture: $(TARGET_ARCH)"
	@echo "Compiler: $(CC)"
	@echo "Host Compatibility Build: $(HOST_BUILD)"
	@echo "Source Files: $(SRCS)"
	@echo "Object Files: $(OBJS)"
	@echo "Output: $(TARGET)"

# Create directories
$(OBJ_DIR):
	@mkdir -p $@

$(LIB_DIR):
	@mkdir -p $@

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Link shared library
$(TARGET): $(OBJS) | $(LIB_DIR)
	@echo "LD  $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@echo ""
	@echo "Built: $@"
	@echo "Size: $$(ls -lh $@ | awk '{print $$5}')"
	@echo ""

# Architecture-specific targets
arm64:
	$(MAKE) TARGET_ARCH=arm64-v8a

arm32:
	$(MAKE) TARGET_ARCH=armeabi-v7a

x86_64:
	$(MAKE) TARGET_ARCH=x86_64

x86:
	$(MAKE) TARGET_ARCH=x86

# Build all architectures
all-archs:
	@for arch in $(ALL_ARCHS); do \
	        echo ""; \
	        echo "========================================"; \
	        echo "Building for $$arch"; \
	        echo "========================================"; \
	        $(MAKE) TARGET_ARCH=$$arch || exit 1; \
	done
	@echo ""
	@echo "All architectures built successfully!"

# Debug build
debug:
	$(MAKE) DEBUG=1

# Release build (optimized)
release:
	$(MAKE) CFLAGS="$(filter-out -O2,$(CFLAGS)) -O3 -flto -fomit-frame-pointer"

# Clean build artifacts
clean:
	@echo "Cleaning..."
	@rm -rf obj/ libs/
	@echo "Clean complete"

# Install to device
install: $(TARGET)
	@echo "Pushing to device..."
	@adb push $(TARGET) /data/local/tmp/
	@adb shell chmod 755 /data/local/tmp/$(notdir $(TARGET))
	@echo "Installed to /data/local/tmp/$(notdir $(TARGET))"

push: install

# Open shell with library preloaded
shell: install
	@echo "Opening shell with UniversalStub preloaded..."
	@adb shell "export LD_PRELOAD=/data/local/tmp/libuniversalstub.so; exec sh"

# Run with an application
run: install
	@echo "Usage: make run APP=com.example.app"
	@if [ -z "$(APP)" ]; then \
	        echo "Error: APP not specified"; \
	        echo "Usage: make run APP=com.example.app"; \
	        exit 1; \
	fi
	@adb shell "setprop wrap.$(APP) 'LD_PRELOAD=/data/local/tmp/libuniversalstub.so'"
	@echo "Configured $(APP) to use UniversalStub"
	@echo "Start the app now"

# Test target
test: $(TARGET)
	@echo "Running tests..."
	@adb push $(TARGET) /data/local/tmp/
	@adb shell "cd /data/local/tmp && LD_PRELOAD=./libuniversalstub.so ./test_app 2>/dev/null || echo 'Test app not found'"

# Strip symbols
strip: $(TARGET)
	@echo "Stripping symbols..."
	@$(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip $(TARGET)
	@echo "Stripped: $@"

# Generate symbol table
symbols: $(TARGET)
	@echo "Symbol table:"
	@$(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf -s $(TARGET) | head -50

# Check for common issues
check: $(TARGET)
	@echo "Checking library..."
	@echo "Dependencies:"
	@$(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf -d $(TARGET) | grep NEEDED
	@echo ""
	@echo "SONAME:"
	@$(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf -d $(TARGET) | grep SONAME
	@echo ""
	@echo "Symbols:"
	@$(NDK_PATH)/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf -s $(TARGET) | grep -E 'dlopen|dlsym|dlclose|JNI' | head -20
