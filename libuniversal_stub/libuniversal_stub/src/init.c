#include "universal_stub.h"

// ==================== CONSTRUCTOR ====================

__attribute__((constructor)) void universal_stub_init(void) {
    LOGI("============================================================");
    LOGI("  %s v%s - Reverse Engineering Toolkit", LIB_NAME, LIB_VERSION);
    LOGI("  Universal Stub & Dependency Resolver");
    LOGI("============================================================");

    // 1. Install crash handlers FIRST (before anything else)
    LOGI("[INIT] Step 1/12: Installing crash handlers...");
    install_crash_handlers();

    // 2. Initialize JIT allocator
    LOGI("[INIT] Step 2/12: Initializing JIT allocator...");
    // JIT allocator is lazy-initialized

    // 3. Scan system configs
    LOGI("[INIT] Step 3/12: Scanning system configs...");
    scan_system_configs();

    // 4. Auto-create missing configs
    LOGI("[INIT] Step 4/12: Auto-creating missing configs...");
    auto_resolve_missing_configs();

    // 5. Check and fix system options (fixed: was calling non-existent function)
    LOGI("[INIT] Step 5/12: Checking system options...");
    check_and_fix_options();

    // 6. Spoof device properties (fixed: now properly delegates to spoof_build_fingerprint)
    LOGI("[INIT] Step 6/12: Spoofing device properties...");
    spoof_device_properties();

    // 7. Hide root traces
    LOGI("[INIT] Step 7/12: Hiding root traces...");
    hide_root_traces();

    // 8. Hide debugger
    LOGI("[INIT] Step 8/12: Hiding debugger...");
    hide_debugger();

    // 9. Bypass emulator detection
    LOGI("[INIT] Step 9/12: Bypassing emulator detection...");
    bypass_emulator_detection();

    // 10. Install global hooks
    LOGI("[INIT] Step 10/12: Installing global hooks...");
    install_global_hooks();

    // 11. Patch security checks
    LOGI("[INIT] Step 11/12: Patching security checks...");
    patch_security_checks();

    // 12. Auto-register JNI methods
    LOGI("[INIT] Step 12/12: Auto-registering JNI methods...");
    JNIEnv *env = get_jni_env();
    if (env) {
        auto_register_missing_jni(env);
    }

    LOGI("============================================================");
    LOGI("  %s initialized successfully!", LIB_NAME);
    LOGI("  Ready to intercept missing libraries and symbols");
    LOGI("  Use 'adb logcat -s UniversalStub:D' to view logs");
    LOGI("============================================================");
}

// ==================== DESTRUCTOR ====================

__attribute__((destructor)) void universal_stub_fini(void) {
    LOGI("[FINI] %s shutting down...", LIB_NAME);

    // Print statistics
    print_crash_stats();

    // Dump generated symbols
    dump_generated_symbols();

    // Dump stubbed libraries
    dump_stubbed_libs();

    // Dump hooks
    dump_hooks();

    // Dump configs
    dump_configs();

    // Free JIT pages
    jit_free_all();

    // Uninstall crash handlers
    uninstall_crash_handlers();

    LOGI("[FINI] %s shutdown complete", LIB_NAME);
}
