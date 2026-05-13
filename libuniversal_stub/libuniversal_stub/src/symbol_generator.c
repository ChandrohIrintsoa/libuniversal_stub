#include "universal_stub.h"

static generated_symbol_t g_symbols[MAX_SYMBOLS];
static int g_symbol_count = 0;
static pthread_mutex_t g_symbol_mutex = PTHREAD_MUTEX_INITIALIZER;

// Hash table for O(1) symbol lookup
static int g_sym_hash[SYM_HASH_SIZE];  // Index into g_symbols, -1 = empty

// Initialize hash table
static void __attribute__((constructor)) sym_hash_init(void) {
    memset(g_sym_hash, -1, sizeof(g_sym_hash));
}

// FNV-1a hash for symbol names
static unsigned int sym_hash_fn(const char *name) {
    unsigned int h = 2166136261u;
    for (const char *p = name; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 16777619u;
    }
    return h & SYM_HASH_MASK;
}

// Insert symbol into hash table
static void sym_hash_insert(int idx) {
    unsigned int h = sym_hash_fn(g_symbols[idx].name);
    g_symbols[idx].hash_next = g_sym_hash[h];
    g_sym_hash[h] = idx;
}

// Find symbol index in hash table (-1 = not found)
static int sym_hash_find(const char *name) {
    unsigned int h = sym_hash_fn(name);
    int idx = g_sym_hash[h];
    while (idx >= 0) {
        if (strcmp(g_symbols[idx].name, name) == 0) {
            return idx;
        }
        idx = g_symbols[idx].hash_next;
    }
    return -1;
}

// Logging stub call handler (moved out of nested function - was a GCC extension bug)
static void log_stub_call(const char *name) {
    LOGV("[STUB-CALL] %s", name);
}

// ==================== TYPE INFERENCE ENGINE ====================

static struct {
    const char *pattern;
    symbol_type_t type;
} type_patterns[] = {
    // JNI patterns
    {"JNI_OnLoad", SYM_TYPE_JNI},
    {"JNI_OnUnload", SYM_TYPE_VOID},
    {"Java_com_", SYM_TYPE_JNI},
    {"Java_org_", SYM_TYPE_JNI},
    {"JNI_VERSION", SYM_TYPE_INT},

    // Init/Create/Alloc patterns -> return pointer
    {"init", SYM_TYPE_PTR},
    {"create", SYM_TYPE_PTR},
    {"alloc", SYM_TYPE_PTR},
    {"new", SYM_TYPE_PTR},
    {"open", SYM_TYPE_PTR},
    {"construct", SYM_TYPE_PTR},
    {"build", SYM_TYPE_PTR},
    {"make", SYM_TYPE_PTR},

    // Get/Check/Is/Has patterns -> return int/bool
    {"get", SYM_TYPE_INT},
    {"check", SYM_TYPE_INT},
    {"is", SYM_TYPE_INT},
    {"has", SYM_TYPE_INT},
    {"can", SYM_TYPE_INT},
    {"should", SYM_TYPE_INT},
    {"verify", SYM_TYPE_INT},
    {"validate", SYM_TYPE_INT},
    {"compare", SYM_TYPE_INT},
    {"find", SYM_TYPE_INT},
    {"search", SYM_TYPE_INT},
    {"count", SYM_TYPE_INT},
    {"size", SYM_TYPE_INT},
    {"length", SYM_TYPE_INT},
    {"index", SYM_TYPE_INT},
    {"pos", SYM_TYPE_INT},

    // Set/Put/Write patterns -> return int (success/fail)
    {"set", SYM_TYPE_INT},
    {"put", SYM_TYPE_INT},
    {"write", SYM_TYPE_INT},
    {"update", SYM_TYPE_INT},
    {"modify", SYM_TYPE_INT},
    {"change", SYM_TYPE_INT},
    {"configure", SYM_TYPE_INT},
    {"setup", SYM_TYPE_INT},

    // Free/Destroy/Close/Clean patterns -> void
    {"free", SYM_TYPE_VOID},
    {"destroy", SYM_TYPE_VOID},
    {"close", SYM_TYPE_VOID},
    {"release", SYM_TYPE_VOID},
    {"dealloc", SYM_TYPE_VOID},
    {"delete", SYM_TYPE_VOID},
    {"remove", SYM_TYPE_VOID},
    {"clear", SYM_TYPE_VOID},
    {"reset", SYM_TYPE_VOID},
    {"cleanup", SYM_TYPE_VOID},
    {"dispose", SYM_TYPE_VOID},
    {"terminate", SYM_TYPE_VOID},
    {"shutdown", SYM_TYPE_VOID},
    {"stop", SYM_TYPE_VOID},
    {"end", SYM_TYPE_VOID},

    // Print/Log/Show patterns -> void
    {"print", SYM_TYPE_VOID},
    {"log", SYM_TYPE_VOID},
    {"display", SYM_TYPE_VOID},
    {"show", SYM_TYPE_VOID},
    {"draw", SYM_TYPE_VOID},
    {"render", SYM_TYPE_VOID},

    // Copy/Move patterns -> return pointer
    {"copy", SYM_TYPE_PTR},
    {"clone", SYM_TYPE_PTR},
    {"dup", SYM_TYPE_PTR},
    {"move", SYM_TYPE_PTR},

    // Convert/Format patterns -> return pointer (string)
    {"to", SYM_TYPE_PTR},
    {"from", SYM_TYPE_PTR},
    {"convert", SYM_TYPE_PTR},
    {"format", SYM_TYPE_PTR},
    {"parse", SYM_TYPE_PTR},
    {"serialize", SYM_TYPE_PTR},
    {"deserialize", SYM_TYPE_PTR},
    {"encode", SYM_TYPE_PTR},
    {"decode", SYM_TYPE_PTR},

    // Callback/Hook/Handler patterns
    {"callback", SYM_TYPE_CALLBACK},
    {"handler", SYM_TYPE_CALLBACK},
    {"listener", SYM_TYPE_CALLBACK},
    {"observer", SYM_TYPE_CALLBACK},
    {"delegate", SYM_TYPE_CALLBACK},
    {"proc", SYM_TYPE_CALLBACK},
    {"function", SYM_TYPE_CALLBACK},

    // String patterns
    {"str", SYM_TYPE_STRING},
    {"string", SYM_TYPE_STRING},
    {"name", SYM_TYPE_STRING},
    {"path", SYM_TYPE_STRING},
    {"url", SYM_TYPE_STRING},
    {"text", SYM_TYPE_STRING},
    {"message", SYM_TYPE_STRING},
    {"error", SYM_TYPE_STRING},
    {"version", SYM_TYPE_STRING},

    // Float/Double patterns
    {"float", SYM_TYPE_FLOAT},
    {"double", SYM_TYPE_DOUBLE},
    {"calc", SYM_TYPE_DOUBLE},
    {"compute", SYM_TYPE_DOUBLE},
    {"math", SYM_TYPE_DOUBLE},
    {"angle", SYM_TYPE_DOUBLE},
    {"ratio", SYM_TYPE_DOUBLE},
    {"percent", SYM_TYPE_DOUBLE},

    // Array/List patterns
    {"array", SYM_TYPE_ARRAY},
    {"list", SYM_TYPE_ARRAY},
    {"vector", SYM_TYPE_ARRAY},
    {"map", SYM_TYPE_ARRAY},
    {"table", SYM_TYPE_ARRAY},
    {"buffer", SYM_TYPE_ARRAY},

    // Struct/Object patterns
    {"struct", SYM_TYPE_STRUCT},
    {"object", SYM_TYPE_STRUCT},
    {"data", SYM_TYPE_STRUCT},
    {"info", SYM_TYPE_STRUCT},
    {"config", SYM_TYPE_STRUCT},
    {"params", SYM_TYPE_STRUCT},
    {"options", SYM_TYPE_STRUCT},
    {"settings", SYM_TYPE_STRUCT},

    {NULL, SYM_TYPE_UNKNOWN}
};

symbol_type_t infer_symbol_type(const char *name) {
    if (!name || !*name) return SYM_TYPE_INT;

    // Check exact matches first
    for (int i = 0; type_patterns[i].pattern; i++) {
        if (strcmp(name, type_patterns[i].pattern) == 0) {
            return type_patterns[i].type;
        }
    }

    // Check substring matches
    char lower_name[MAX_SYMBOL_LEN];
    int j = 0;
    for (const char *p = name; *p && j < MAX_SYMBOL_LEN - 1; p++) {
        lower_name[j++] = tolower(*p);
    }
    lower_name[j] = '\0';

    for (int i = 0; type_patterns[i].pattern; i++) {
        if (strstr(lower_name, type_patterns[i].pattern) != NULL) {
            return type_patterns[i].type;
        }
    }

    // Default based on naming conventions
    if (name[0] == 'g' && (name[1] == 'e' || name[1] == 'e')) {
        return SYM_TYPE_INT;  // get* -> int
    }
    if (name[0] == 's' && name[1] == 'e') {
        return SYM_TYPE_INT;  // set* -> int
    }
    if (name[0] == 'i' && name[1] == 's') {
        return SYM_TYPE_INT;  // is* -> bool/int
    }
    if (name[0] == 'h' && name[1] == 'a') {
        return SYM_TYPE_INT;  // has* -> bool/int
    }
    if (name[0] == 'c' && name[1] == 'a') {
        return SYM_TYPE_INT;  // can* -> bool/int
    }

    return SYM_TYPE_INT;  // Default: int
}

// ==================== ARM64 HELPER: Load 64-bit address into register ====================

// Emit instructions to load a 64-bit address into a register
// Returns the number of instructions emitted
static int emit_load_addr(uint32_t *code, int reg, uint64_t addr) {
    int idx = 0;

    // movz x(reg), #addr[15:0]
    code[idx++] = 0xD2800000 | ((addr & 0xFFFF) << 5) | reg;

    // movk x(reg), #addr[31:16], lsl #16
    if (addr >> 16) {
        code[idx++] = 0xF2A00000 | (((addr >> 16) & 0xFFFF) << 5) | reg;
    }

    // movk x(reg), #addr[47:32], lsl #32
    if (addr >> 32) {
        code[idx++] = 0xF2C00000 | (((addr >> 32) & 0xFFFF) << 5) | reg;
    }

    // movk x(reg), #addr[63:48], lsl #48
    if (addr >> 48) {
        code[idx++] = 0xF2E00000 | (((addr >> 48) & 0xFFFF) << 5) | reg;
    }

    return idx;
}

// ==================== ARM64 JIT CODE GENERATOR ====================

static void *generate_arm64_stub(symbol_type_t type, const char *name) {
    void *mem = jit_alloc(JIT_PAGE_SIZE);
    if (!mem) return NULL;

    uint32_t *code = (uint32_t *)mem;
    int idx = 0;

    // Prologue: stp x29, x30, [sp, #-16]!
    code[idx++] = 0xA9BF7BFD;
    // mov x29, sp
    code[idx++] = 0x910003FD;

    switch (type) {
        case SYM_TYPE_VOID:
            // Just return
            code[idx++] = 0xA8C17BFD;  // ldp x29, x30, [sp], #16
            code[idx++] = 0xD65F03C0;  // ret
            break;

        case SYM_TYPE_INT:
            // mov x0, #1 (return 1/true for security checks)
            code[idx++] = 0xD2800002;  // mov x0, #1
            code[idx++] = 0xA8C17BFD;  // ldp x29, x30, [sp], #16
            code[idx++] = 0xD65F03C0;  // ret
            break;

        case SYM_TYPE_PTR:
            // mov x0, #0 (NULL)
            code[idx++] = 0xD2800000;
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;

        case SYM_TYPE_FLOAT:
            // fmov s0, #0.0
            code[idx++] = 0x1E2703E0;
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;

        case SYM_TYPE_DOUBLE:
            // fmov d0, #0.0
            code[idx++] = 0x1E6E1000;
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;

        case SYM_TYPE_STRING:
            // Return empty string pointer
            {
                static char empty_str[1] = {0};
                idx += emit_load_addr(&code[idx], 0, (uint64_t)empty_str);
                code[idx++] = 0xA8C17BFD;
                code[idx++] = 0xD65F03C0;
            }
            break;

        case SYM_TYPE_JNI:
            // Return JNI_VERSION_1_6 (0x10006)
            code[idx++] = 0xD280000C;  // movz x0, #0x0006
            code[idx++] = 0xF2A00020;  // movk x0, #0x0001, lsl #16
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;

        case SYM_TYPE_CALLBACK:
            // Call the callback with args passed through, return 0
            code[idx++] = 0xD2800000;  // mov x0, #0
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;

        case SYM_TYPE_STRUCT:
        case SYM_TYPE_ARRAY:
            // Return NULL (caller should check)
            code[idx++] = 0xD2800000;
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;

        default:
            code[idx++] = 0xD2800000;
            code[idx++] = 0xA8C17BFD;
            code[idx++] = 0xD65F03C0;
            break;
    }

    __builtin___clear_cache(mem, (char *)mem + JIT_PAGE_SIZE);

    LOGI("[GEN] Generated ARM64 stub for '%s' (type=%d) at %p", name, type, mem);
    return mem;
}

// ==================== LOGGING STUB (Fixed: no nested function) ====================

static void *generate_logging_stub(symbol_type_t type, const char *name) {
    void *mem = jit_alloc(JIT_PAGE_SIZE);
    if (!mem) return NULL;

    // This stub logs the call then returns a default value.
    // We use the global log_stub_call() function instead of a nested function.

    uint32_t *code = (uint32_t *)mem;
    int idx = 0;

    // Save registers
    code[idx++] = 0xA9BF7BFD;  // stp x29, x30, [sp, #-16]!
    code[idx++] = 0x910003FD;  // mov x29, sp

    // Save x0-x7 (arguments)
    code[idx++] = 0xA9BF07E0;  // stp x0, x1, [sp, #-16]!
    code[idx++] = 0xA9BF0FE2;  // stp x2, x3, [sp, #-16]!
    code[idx++] = 0xA9BF17E4;  // stp x4, x5, [sp, #-16]!
    code[idx++] = 0xA9BF1FE6;  // stp x6, x7, [sp, #-16]!

    // Load name string address into x0 (fixed: complete 4-step address load)
    idx += emit_load_addr(&code[idx], 0, (uint64_t)name);

    // Load log_stub_call address into x8 (fixed: complete 4-step address load)
    idx += emit_load_addr(&code[idx], 8, (uint64_t)log_stub_call);

    // Call log_stub_call
    code[idx++] = 0xD63F0100;  // blr x8

    // Restore x0-x7
    code[idx++] = 0xA8C11FE6;  // ldp x6, x7, [sp], #16
    code[idx++] = 0xA8C117E4;  // ldp x4, x5, [sp], #16
    code[idx++] = 0xA8C10FE2;  // ldp x2, x3, [sp], #16
    code[idx++] = 0xA8C107E0;  // ldp x0, x1, [sp], #16

    // Return value based on type
    switch (type) {
        case SYM_TYPE_VOID:
            break;
        case SYM_TYPE_INT:
            code[idx++] = 0xD2800002;  // mov x0, #1 (return 1/true for checks)
            break;
        case SYM_TYPE_PTR:
            code[idx++] = 0xD2800000;  // mov x0, #0
            break;
        case SYM_TYPE_FLOAT:
            code[idx++] = 0x1E2703E0;  // fmov s0, #0.0
            break;
        case SYM_TYPE_DOUBLE:
            code[idx++] = 0x1E6E1000;  // fmov d0, #0.0
            break;
        default:
            code[idx++] = 0xD2800000;
            break;
    }

    code[idx++] = 0xA8C17BFD;  // ldp x29, x30, [sp], #16
    code[idx++] = 0xD65F03C0;  // ret

    __builtin___clear_cache(mem, (char *)mem + JIT_PAGE_SIZE);

    LOGI("[GEN] Generated logging stub for '%s' at %p", name, mem);
    return mem;
}

// ==================== PUBLIC API ====================

void *generate_jit_function(symbol_type_t type, const char *name) {
    if (!name) return NULL;

    // Use logging stubs for known security/verification functions
    if (strstr(name, "verify") || strstr(name, "check") ||
        strstr(name, "validate") || strstr(name, "security") ||
        strstr(name, "integrity") || strstr(name, "attestation") ||
        strstr(name, "detect") || strstr(name, "isRoot") ||
        strstr(name, "isEmulator") || strstr(name, "isDebug")) {
        return generate_logging_stub(type, name);
    }

    return generate_arm64_stub(type, name);
}

void *generate_symbol(const char *name, symbol_type_t suggested_type) {
    pthread_mutex_lock(&g_symbol_mutex);

    // Check if already generated (O(1) hash lookup)
    int existing = sym_hash_find(name);
    if (existing >= 0) {
        pthread_mutex_unlock(&g_symbol_mutex);
        return g_symbols[existing].addr;
    }

    if (g_symbol_count >= MAX_SYMBOLS) {
        LOGE("[GEN] Symbol table full!");
        pthread_mutex_unlock(&g_symbol_mutex);
        return NULL;
    }

    symbol_type_t type = (suggested_type != SYM_TYPE_UNKNOWN) ?
                         suggested_type : infer_symbol_type(name);

    void *addr = generate_jit_function(type, name);

    if (addr) {
        int idx = g_symbol_count;
        strncpy(g_symbols[idx].name, name, MAX_SYMBOL_LEN - 1);
        g_symbols[idx].name[MAX_SYMBOL_LEN - 1] = '\0';
        g_symbols[idx].addr = addr;
        g_symbols[idx].type = type;
        g_symbols[idx].is_generated = true;
        g_symbols[idx].is_hooked = false;
        g_symbols[idx].original_addr = NULL;
        g_symbols[idx].call_count = 0;
        g_symbols[idx].last_call_time = 0;
        strncpy(g_symbols[idx].source_lib, "generated", MAX_LIB_NAME - 1);
        g_symbols[idx].source_lib[MAX_LIB_NAME - 1] = '\0';
        g_symbols[idx].hash_next = -1;
        sym_hash_insert(idx);
        g_symbol_count++;

        LOGI("[GEN] Registered symbol #%d: '%s' (type=%d) at %p",
             g_symbol_count, name, type, addr);
    }

    pthread_mutex_unlock(&g_symbol_mutex);
    return addr;
}

void *get_or_generate_symbol(const char *name, symbol_type_t type) {
    pthread_mutex_lock(&g_symbol_mutex);

    // O(1) hash lookup
    int idx = sym_hash_find(name);
    if (idx >= 0) {
        void *addr = g_symbols[idx].addr;
        g_symbols[idx].call_count++;
        g_symbols[idx].last_call_time = get_timestamp_ns();
        pthread_mutex_unlock(&g_symbol_mutex);
        return addr;
    }

    pthread_mutex_unlock(&g_symbol_mutex);
    return generate_symbol(name, type);
}

void *generate_jni_method(JNIEnv *env, const char *class_name,
                          const char *method_name, const char *sig) {
    LOGI("[JNI] Generating method: %s->%s%s", class_name, method_name, sig);

    void *fn = generate_symbol(method_name, SYM_TYPE_JNI);
    if (!fn) return NULL;

    JNINativeMethod method = {
        .name = (char *)method_name,
        .signature = (char *)sig,
        .fnPtr = fn
    };

    jclass cls = (*env)->FindClass(env, class_name);
    if (!cls) {
        LOGW("[JNI] Class not found: %s", class_name);
        // Try to find via system class loader
        jclass cl_cls = (*env)->FindClass(env, "java/lang/ClassLoader");
        if (cl_cls) {
            jmethodID get_sys_cl = (*env)->GetStaticMethodID(env, cl_cls,
                "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
            if (get_sys_cl) {
                jobject sys_cl = (*env)->CallStaticObjectMethod(env, cl_cls, get_sys_cl);
                if (sys_cl) {
                    jmethodID load_cls = (*env)->GetMethodID(env, cl_cls,
                        "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
                    if (load_cls) {
                        jstring name = (*env)->NewStringUTF(env, class_name);
                        if (name) {
                            cls = (jclass)(*env)->CallObjectMethod(env, sys_cl, load_cls, name);
                            (*env)->DeleteLocalRef(env, name);
                        }
                    }
                    (*env)->DeleteLocalRef(env, sys_cl);
                }
            }
            (*env)->DeleteLocalRef(env, cl_cls);
        }
    }

    if (cls) {
        jint ret = (*env)->RegisterNatives(env, cls, &method, 1);
        if (ret == 0) {
            LOGI("[JNI] Successfully registered %s->%s", class_name, method_name);
        } else {
            LOGW("[JNI] RegisterNatives failed: %d", ret);
        }
        (*env)->DeleteLocalRef(env, cls);
    }

    return fn;
}

void *generate_callback_wrapper(const char *name, int arg_count, symbol_type_t ret_type) {
    LOGI("[CALLBACK] Generating wrapper '%s' (%d args, ret=%d)", name, arg_count, ret_type);

    // Fixed: check if already registered to avoid duplicate JIT allocation
    pthread_mutex_lock(&g_symbol_mutex);
    int existing = sym_hash_find(name);
    if (existing >= 0) {
        void *addr = g_symbols[existing].addr;
        pthread_mutex_unlock(&g_symbol_mutex);
        LOGI("[CALLBACK] Wrapper already exists: '%s' at %p", name, addr);
        return addr;
    }
    pthread_mutex_unlock(&g_symbol_mutex);

    void *mem = jit_alloc(JIT_PAGE_SIZE);
    if (!mem) return NULL;

    uint32_t *code = (uint32_t *)mem;
    int idx = 0;

    // Prologue
    code[idx++] = 0xA9BF7BFD;  // stp x29, x30, [sp, #-16]!
    code[idx++] = 0x910003FD;  // mov x29, sp

    // For callbacks, we just pass through and return default
    switch (ret_type) {
        case SYM_TYPE_VOID:
            break;
        case SYM_TYPE_INT:
        case SYM_TYPE_PTR:
            code[idx++] = 0xD2800000;
            break;
        case SYM_TYPE_FLOAT:
            code[idx++] = 0x1E2703E0;
            break;
        case SYM_TYPE_DOUBLE:
            code[idx++] = 0x1E6E1000;
            break;
        default:
            code[idx++] = 0xD2800000;
            break;
    }

    code[idx++] = 0xA8C17BFD;
    code[idx++] = 0xD65F03C0;

    __builtin___clear_cache(mem, (char *)mem + JIT_PAGE_SIZE);

    // Fixed: register with the actual JIT address (not a new generated stub)
    pthread_mutex_lock(&g_symbol_mutex);
    if (g_symbol_count < MAX_SYMBOLS) {
        int slot = g_symbol_count;
        strncpy(g_symbols[slot].name, name, MAX_SYMBOL_LEN - 1);
        g_symbols[slot].name[MAX_SYMBOL_LEN - 1] = '\0';
        g_symbols[slot].addr = mem;
        g_symbols[slot].type = ret_type;
        g_symbols[slot].is_generated = true;
        g_symbols[slot].is_hooked = false;
        g_symbols[slot].original_addr = NULL;
        g_symbols[slot].call_count = 0;
        g_symbols[slot].last_call_time = 0;
        strncpy(g_symbols[slot].source_lib, "callback", MAX_LIB_NAME - 1);
        g_symbols[slot].source_lib[MAX_LIB_NAME - 1] = '\0';
        g_symbols[slot].hash_next = -1;
        sym_hash_insert(slot);
        g_symbol_count++;
    }
    pthread_mutex_unlock(&g_symbol_mutex);

    return mem;
}

int get_symbol_info(const char *name, generated_symbol_t *out) {
    if (!name) return 0;

    pthread_mutex_lock(&g_symbol_mutex);

    // O(1) hash lookup
    int idx = sym_hash_find(name);
    if (idx >= 0) {
        if (out) *out = g_symbols[idx];
        pthread_mutex_unlock(&g_symbol_mutex);
        return 1;
    }

    pthread_mutex_unlock(&g_symbol_mutex);
    return 0;
}

void dump_generated_symbols(void) {
    pthread_mutex_lock(&g_symbol_mutex);

    LOGI("[DUMP] === Generated Symbols (%d total) ===", g_symbol_count);
    for (int i = 0; i < g_symbol_count; i++) {
        LOGI("[DUMP] #%d: '%s' type=%d addr=%p calls=%lu",
             i, g_symbols[i].name, g_symbols[i].type,
             g_symbols[i].addr, (unsigned long)g_symbols[i].call_count);
    }

    pthread_mutex_unlock(&g_symbol_mutex);
}
