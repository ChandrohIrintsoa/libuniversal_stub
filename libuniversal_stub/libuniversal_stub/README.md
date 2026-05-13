# UniversalStub v5.0.0-RE

**Universal stub/proxy library for Android - Specialized Reverse Engineering**

> **Warning**: This tool is intended for security research, debugging, and legal reverse engineering only. Using it to bypass the security of third-party applications without authorization is illegal.

---

## Features

### 1. Missing Library Interception
- **Auto-detection** of missing libs (`libpairipcore.so`, etc.)
- **Dummy handle creation** for absent libs
- **List of 100+ known libs** (Pairip, SafetyNet, Play Integrity, DRM, KernelSU, APatch, Magisk Delta, etc.)
- **2025+ additions**: Play Integrity v2, device attestation, identity credential libs

### 2. JIT Symbol Generator (Just-In-Time)
- **Automatic generation** of ARM64 stub functions
- **Type inference** by analyzing symbol name patterns
- **131072 max symbols** in memory
- **O(1) hash lookup** via FNV-1a hash table (new in v5)
- **Supported types**: void, int, ptr, float, double, string, JNI, callback, struct, array
- **Complete 64-bit address loading** in generated code (fixed from v3)

### 3. Advanced Crash Handler
- **Interception** of SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL, SIGTRAP
- **Automatic recovery**:
  - Null pointer -> guard page mapping
  - SIGABRT -> instruction skip
  - SIGFPE -> return 0
  - SIGBUS/SIGILL -> skip instruction
- **Backtrace** with symbol resolution
- **ARM64 register dump**
- **Crash statistics** (fixed: no more out-of-bounds access)

### 4. Hooking Framework
- **PLT hooking**: intercept dynamic call entries (now properly implemented)
- **GOT hooking**: intercept global offset table entries
- **Inline hooking**: instruction replacement with proper trampoline
- **Syscall hooking**: system call interception
- **JNI hooking**: native method interception
- **4096 max hooks**

### 5. Configuration Resolver
- **Automatic scan** of system configs
- **Auto-creation** of 12+ default configs:
  - Pairip properties
  - DRM config
  - SafetyNet / Play Integrity
  - Device properties (updated to Android 15 / Pixel 9 Pro)
  - SSL config
  - License config
  - Root hide config (with KernelSU/APatch support)
  - KernelSU hide config (new in v5)
  - APatch hide config (new in v5)
- **Safe directory creation** (fixed: no more shell injection via system())

### 6. Anti-Detection
- **Root hiding**: files, packages, properties (including KernelSU/APatch/Magisk Delta)
- **Debugger hiding**: TracerPid, process list
- **Device spoofing**: fingerprint, model, brand (updated to Pixel 9 Pro / Android 15)
- **Emulator bypass**: properties, files
- **SSL pinning patch**: TrustManager, OkHttp
- **Security check patch**: ptrace, root, debug
- **Safe property setting**: Uses `__system_property_set()` instead of `system("setprop")` (fixed in v5)

### 7. Complete JNI Stubs
- **Pairip**: checkIntegrity, isDeviceSecure, verifySignature, etc.
- **SafetyNet**: getJwsResult, attest
- **Play Integrity**: requestIntegrityToken (including StandardIntegrityManager)
- **Play Integrity v2**: showDialog (new in v5)
- **SecurityChecker**: isRooted, isEmulator, isDebugged, etc.
- **RootChecker**: isRooted, checkSuBinary, checkTestKeys
- **DebugDetector**: isDebuggerAttached, isDebugBuild
- **DRM**: initialize, acquireLicense, isProvisioned
- **License**: verifyLicense, isLicensed, isPremium, getLicenseType
- **SSL**: disablePinning, isPinningEnabled
- **Certificate**: validate, verifyChain
- **AntiCheat**: detectCheat, detectMod, detectSpeedHack, detectMemoryHack
- **Analytics**: trackEvent, trackScreen, setUserProperty
- **KernelSU**: isKernelSUAvaliable, isSafeMode
- **Identity Credential**: isHardwareBacked, createCredential (new in v5)
- **Biometric**: canAuthenticate (new in v5)
- **HideMyAppList**: isHidden (new in v5)
- **Magisk Delta/Kitsune**: isRootAvailable (new in v5)

---

## Changes from v4.0.0 to v5.0.0

| Change | Description | Impact |
|--------|-------------|--------|
| **Symbol hash table** | FNV-1a hash table with O(1) lookup replacing O(n) linear scan | Major performance improvement for symbol resolution |
| **Android 15 support** | Device fingerprint updated to Pixel 9 Pro / API 35 | Current device profiles |
| **Shell injection fix** | `spoof_build_fingerprint()` replaced `system("setprop")` with `__system_property_set()` | Security critical fix |
| **dlerror semantics** | `universal_dlerror()` now properly clears buffer after read (matching dlerror behavior) | Correctness fix |
| **Callback wrapper dedup** | `generate_callback_wrapper()` no longer creates duplicate JIT allocations | Memory leak fix |
| **enumerate_imports()** | Full implementation of ELF import enumeration (was a stub returning 0) | Functionality improvement |
| **20+ new known libs** | Play Integrity v2, device attestation, identity credential, Kotlin/Compose libs | Better coverage |
| **New root indicators** | Magisk Delta, Kitsune, APatch daemon, newer Magisk paths | Better coverage |
| **New root packages** | Magisk Delta/Kitsune, HideMyAppList, KernelSU nobackup | Better coverage |
| **New JNI stubs** | Identity Credential, Play Integrity v2, Biometric, HideMyAppList, Magisk Delta | Better coverage |
| **New configs** | KernelSU and APatch auto-generated config files | Better coverage |
| **NDK r28 support** | Added NDK r28 path to Makefile auto-detection | Build compatibility |
| **New Frida hooks** | KernelSU and APatch bypass hooks in companion script | Better coverage |
| **New security patterns** | shamiko, kernelsu, apatch, zksig, device_attest, app_attest patterns | Better detection |

---

## Bug Fixes from v4.0.0

| Bug | Description | Fix |
|-----|-------------|-----|
| **Shell injection in setprop** | `spoof_build_fingerprint()` used `system("setprop ...")` with property values that could be crafted for injection | Replaced with `__system_property_set()` direct API call |
| **dlerror buffer never cleared** | `universal_dlerror()` returned its internal buffer but never cleared it, violating dlerror() semantics | Copy to return buffer and clear internal buffer after read |
| **Duplicate JIT allocation** | `generate_callback_wrapper()` called `generate_symbol()` which created a second JIT stub, wasting memory and returning wrong address | Direct hash table registration with the actual JIT address |
| **O(n) symbol lookup** | All symbol lookups used linear scan through 131072 entries | Added FNV-1a hash table for O(1) average lookup |
| **Unimplemented enumerate_imports()** | Returned 0 without doing any work | Full implementation parsing DT_SYMTAB, DT_RELA, DT_JMPREL |

---

## Bug Fixes from v3.0.0 (carried forward)

| Bug | Description | Fix |
|-----|-------------|-----|
| **Out-of-bounds access** | `crash_handler.c` accessed `by_type[CRASH_TYPE_HEAP_CORRUPTION]` beyond array bounds | Added `CRASH_TYPE_MAX` enum sentinel, bounds checking |
| **Duplicate static functions** | `is_security_symbol()` defined in both `universal_stub.c` and `library_manager.c` | Made non-static in `library_manager.c`, declared in header |
| **Overly broad security patterns** | `is_security_symbol()` matched "be", "have", "do", "make", etc. | Reduced to focused security-related patterns only |
| **Nested function** | `generate_logging_stub()` used GCC nested function extension | Moved `log_stub_call()` to file scope |
| **Incomplete address loading** | Logging stubs only loaded 2 of 4 possible 16-bit address chunks | Added `emit_load_addr()` helper with full 4-step loading |
| **Missing hook_plt_entry** | Declared in header but never implemented | Full PLT hooking implementation with DT_JMPREL parsing |
| **Broken trampoline** | Inline hook trampoline used `ldr x16, =return_addr` without storing the address | Changed to `movz/movk x16` pattern for proper address embedding |
| **JIT memory leak** | `jit_free_all()` always unmapped `JIT_PAGE_SIZE` ignoring actual allocation | Added `page_sizes[]` tracking, proper `munmap` with correct sizes |
| **Missing function declarations** | Many functions called across translation units without header declarations | Added all missing declarations to `universal_stub.h` |
| **Function name mismatches** | `init.c` called `spoof_device_properties()` which didn't exist | Implemented `spoof_device_properties()` as delegate to `spoof_build_fingerprint()` |
| **Unimplemented functions** | `validate_permissions()`, `auto_grant_permissions()`, etc. declared but not implemented | Full implementations added |
| **Shell injection risk** | `config_resolver.c` used `system("mkdir -p ...")` with unsanitized paths | Replaced with recursive `mkdir()` calls |
| **JNI null safety** | Several JNI stubs didn't check for null class references | Added null checks before `DeleteLocalRef` |
| **Init step numbering** | Steps counted "11/10" and "12/10" | Fixed to correct "11/12" and "12/12" |
| **Thread-unsafe static buffers** | `get_config_string/int/bool` used shared static buffers | Changed to `__thread` local storage |

---

## Project Structure

```
libuniversal_stub/
├── include/
│   └── universal_stub.h          # Main header (with hash table support)
├── src/
│   ├── init.c                     # Constructor/Destructor
│   ├── universal_stub.c           # Core: dlopen/dlsym interception, ELF parser, utilities
│   ├── symbol_generator.c         # JIT symbol generator (with O(1) hash lookup)
│   ├── library_manager.c          # Missing lib manager (100+ known libs)
│   ├── hook_framework.c           # Hooking framework
│   ├── crash_handler.c            # Crash handler
│   ├── config_resolver.c          # Configuration resolver (12+ configs)
│   ├── anti_detection.c           # Anti-detection & JNI bridge
│   ├── jni_stubs.c                # Specific JNI stubs (18+ categories)
│   └── jit_allocator.c            # JIT allocator
├── tools/
│   ├── inject.sh                  # Injection script
│   └── frida_hook.js              # Frida companion script (with KernelSU/APatch bypass)
├── Makefile                       # Build system (NDK r25-r28)
└── README.md                      # Documentation
```

---

## Building

### Prerequisites
- Android NDK (r25 or later, r26/r27/r28 recommended)
- Make
- ADB (for deployment)

### Simple build (ARM64)
```bash
cd libuniversal_stub
make arm64
```

### All architectures
```bash
make all-archs
```

### Debug build
```bash
make debug
```

### Optimized release build
```bash
make release
```

### Architecture-specific
```bash
make arm64     # ARM64
make arm32     # ARMv7
make x86_64    # x86_64
make x86       # x86
```

---

## Usage

### Method 1: LD_PRELOAD (recommended)

```bash
# Push to device
adb push libs/arm64-v8a/libuniversalstub.so /data/local/tmp/

# Direct usage
adb shell "LD_PRELOAD=/data/local/tmp/libuniversalstub.so ./my_app"

# Or via wrap property (requires debuggable or root)
adb shell setprop wrap.com.myapp "LD_PRELOAD=/data/local/tmp/libuniversalstub.so"
```

### Method 2: Injection script

```bash
# Install the library
adb push tools/inject.sh /data/local/tmp/
adb shell "chmod 755 /data/local/tmp/inject.sh"

# Inject into an app
adb shell "/data/local/tmp/inject.sh com.myapp -f -k"

# View logs
adb shell "/data/local/tmp/inject.sh com.myapp -l"

# Check status
adb shell "/data/local/tmp/inject.sh -s"

# Check environment
adb shell "/data/local/tmp/inject.sh -c"
```

### Method 3: Frida + UniversalStub

```bash
# Launch with Frida and UniversalStub
frida -U -f com.myapp -l tools/frida_hook.js --no-pause \
    -e "const LD_PRELOAD='/data/local/tmp/libuniversalstub.so'"
```

### Method 4: APK inclusion

1. Copy `libuniversalstub.so` into `lib/arm64-v8a/`
2. Modify `AndroidManifest.xml`:
   ```xml
   <application
       android:extractNativeLibs="true"
       ... >
   ```
3. Rebuild the APK

---

## Logs & Monitoring

```bash
# View real-time logs
adb logcat -s UniversalStub:D

# View all log levels
adb logcat -s UniversalStub:V

# Filter generated symbols
adb logcat -s UniversalStub:D | grep "\[GEN\]"

# Filter crashes
adb logcat -s UniversalStub:D | grep "\[CRASH\]"

# Filter hooks
adb logcat -s UniversalStub:D | grep "\[HOOK\]"
```

---

## Use Cases

### Case 1: Replacing `libpairipcore.so`
```
The app loads libpairipcore.so -> UniversalStub creates a stub
All symbols are auto-generated with default values
JNI methods are auto-registered
```

### Case 2: Bypassing security checks
```
Verification functions always return "pass"
isRooted() -> false
isEmulator() -> false
isDebugged() -> false
checkIntegrity() -> true
```

### Case 3: Crash recovery
```
SIGSEGV on null pointer -> auto guard page mapping
SIGABRT -> instruction skip
The app continues running instead of crashing
```

### Case 4: Function hooking
```
Hook GOT entry to intercept calls
Replace security functions
Log all sensitive calls
```

---

## Configuration

Configs are automatically created in:
- `/data/local/tmp/universal_stub/`
- `/sdcard/.universal_stub/`
- `/data/misc/`

You can modify the `.prop` and `.conf` files to adjust behavior.

---

## Limitations & Warnings

1. **Default values**: Generated functions return neutral values (0, NULL, true)
2. **Complex functions**: Some functions require manual stubs
3. **Architecture**: Optimized for ARM64, ARMv7/x86/x86_64 supported
4. **Root required**: Some features require root
5. **SELinux**: May block some operations on recent Android
6. **Performance**: Slight overhead due to interception

---

## Statistics

```
- 100+ known libs supported
- 131072 max symbols
- 512 max libs
- 4096 max hooks
- 12+ auto-generated configs
- 6 crash types intercepted
- 18+ categories of JNI stubs
- 4 architectures supported
- Android API 21+ support
- O(1) symbol lookup via hash table
```

---

## Resources

- [Android NDK](https://developer.android.com/ndk)
- [Frida](https://frida.re)
- [ELF Format](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [ARM64 Instruction Set](https://developer.arm.com/documentation)

---

## License

**Educational and research use only.**

The author is not responsible for illegal use of this tool.

---

## Credits

Developed for the Android reverse engineering community.
Version 5.0.0-RE - Reverse Engineering Toolkit
