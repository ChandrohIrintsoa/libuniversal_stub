#ifndef UNIVERSAL_STUB_H
#define UNIVERSAL_STUB_H

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <jni.h>
#include <sys/mman.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <link.h>
#include <elf.h>
#include <ucontext.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <regex.h>
#include <ctype.h>

#define LOG_TAG "UniversalStub"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define LIB_VERSION "5.0.0-RE"
#define LIB_NAME "UniversalStub-RE"

#define MAX_SYMBOLS 131072
#define MAX_LIBS 512
#define MAX_CONFIG_PATHS 128
#define MAX_HOOKS 4096
#define MAX_BACKTRACE 128
#define MAX_PATH_LEN 1024
#define MAX_SYMBOL_LEN 512
#define MAX_LIB_NAME 256
#define JIT_PAGE_SIZE 4096
#define MAX_JIT_PAGES 256
#define MAX_CRASH_TYPES 9

// Symbol hash table for O(1) lookup
#define SYM_HASH_SIZE 8192  // Must be power of 2
#define SYM_HASH_MASK (SYM_HASH_SIZE - 1)

typedef enum {
    SYM_TYPE_VOID = 0,
    SYM_TYPE_INT,
    SYM_TYPE_PTR,
    SYM_TYPE_FLOAT,
    SYM_TYPE_DOUBLE,
    SYM_TYPE_STRING,
    SYM_TYPE_JNI,
    SYM_TYPE_CALLBACK,
    SYM_TYPE_STRUCT,
    SYM_TYPE_ARRAY,
    SYM_TYPE_UNKNOWN
} symbol_type_t;

typedef enum {
    HOOK_TYPE_PLT = 0,
    HOOK_TYPE_INLINE,
    HOOK_TYPE_GOT,
    HOOK_TYPE_SYSCALL,
    HOOK_TYPE_JNI,
    HOOK_TYPE_VTABLE
} hook_type_t;

typedef enum {
    CRASH_TYPE_NONE = 0,
    CRASH_TYPE_SIGSEGV,
    CRASH_TYPE_SIGABRT,
    CRASH_TYPE_SIGFPE,
    CRASH_TYPE_SIGBUS,
    CRASH_TYPE_SIGILL,
    CRASH_TYPE_SIGTRAP,
    CRASH_TYPE_STACK_OVERFLOW,
    CRASH_TYPE_HEAP_CORRUPTION,
    CRASH_TYPE_MAX
} crash_type_t;

typedef struct generated_symbol_node {
    char name[MAX_SYMBOL_LEN];
    void *addr;
    symbol_type_t type;
    bool is_generated;
    bool is_hooked;
    void *original_addr;
    uint64_t call_count;
    uint64_t last_call_time;
    char source_lib[MAX_LIB_NAME];
    int hash_next;  // Index of next entry in hash chain (-1 = end)
} generated_symbol_t;

typedef struct {
    char name[MAX_LIB_NAME];
    char replacement[MAX_LIB_NAME];
    bool is_stubbed;
    void *handle;
    void *base_addr;
    size_t size;
    int ref_count;
    time_t load_time;
    bool is_fake;
    Elf64_Ehdr *elf_header;
    Elf64_Shdr *section_headers;
    char *section_names;
    Elf64_Sym *symtab;
    size_t symtab_count;
    char *strtab;
} missing_lib_t;

typedef struct {
    char key[256];
    char value[2048];
    char source[MAX_PATH_LEN];
    time_t created;
    bool is_auto_generated;
} auto_config_t;

typedef struct {
    char lib_name[MAX_LIB_NAME];
    char symbol[MAX_SYMBOL_LEN];
    void *new_addr;
    void *original_addr;
    void **trampoline;
    hook_type_t type;
    bool active;
    uint64_t hit_count;
    uint64_t last_hit;
} hook_entry_t;

typedef struct {
    crash_type_t type;
    int signal;
    siginfo_t *info;
    ucontext_t *context;
    void **backtrace;
    int backtrace_size;
    char description[512];
    time_t timestamp;
    bool handled;
    bool recovered;
} crash_context_t;

typedef struct {
    void *pages[MAX_JIT_PAGES];
    size_t page_sizes[MAX_JIT_PAGES];
    int count;
    size_t used;
    pthread_mutex_t mutex;
} jit_allocator_t;

typedef struct {
    char class_name[256];
    char method_name[256];
    char signature[512];
    void *fn_ptr;
    bool registered;
} jni_native_entry_t;

// ============ CORE FUNCTIONS ============
void *universal_dlsym(void *handle, const char *symbol);
void *universal_dlopen(const char *filename, int flags);
int universal_dlclose(void *handle);
char *universal_dlerror(void);

// ============ SYMBOL GENERATOR ============
void *generate_symbol(const char *name, symbol_type_t suggested_type);
void *generate_jit_function(symbol_type_t type, const char *name);
void *generate_jni_method(JNIEnv *env, const char *class_name,
                          const char *method_name, const char *sig);
void *generate_callback_wrapper(const char *name, int arg_count, symbol_type_t ret_type);
symbol_type_t infer_symbol_type(const char *name);
void *get_or_generate_symbol(const char *name, symbol_type_t type);
int get_symbol_info(const char *name, generated_symbol_t *out);
void dump_generated_symbols(void);

// ============ JIT ALLOCATOR ============
void *jit_alloc(size_t size);
void jit_free_all(void);
void *jit_write_code(void *dest, const void *src, size_t size);

// ============ CONFIG RESOLVER ============
int resolve_config(const char *key, char *out_value, size_t out_size);
int create_default_config(const char *path, const char *content);
int scan_system_configs(void);
void auto_resolve_missing_configs(void);
int load_config_file(const char *path);
int save_config_file(const char *path);
const char *get_config_string(const char *key, const char *default_value);
int get_config_int(const char *key, int default_value);
bool get_config_bool(const char *key, bool default_value);
void dump_configs(void);

// ============ PERMISSION VALIDATOR ============
int validate_permissions(JNIEnv *env);
int auto_grant_permissions(JNIEnv *env);
int check_and_fix_options(void);
int spoof_device_properties(void);
int bypass_safety_checks(JNIEnv *env);

// ============ HOOK FRAMEWORK ============
int hook_plt_entry(const char *lib_name, const char *symbol,
                   void *new_addr, void **old_addr);
int hook_inline(void *target, void *new_addr, void **trampoline);
int hook_got_entry(const char *lib_name, const char *symbol,
                     void *new_addr, void **old_addr);
int hook_syscall(long syscall_num, void *new_handler);
int install_global_hooks(void);
int remove_hook(int hook_id);
void *get_hook_trampoline(int hook_id);
void dump_hooks(void);

// ============ CRASH HANDLER ============
void install_crash_handlers(void);
void uninstall_crash_handlers(void);
void crash_handler(int sig, siginfo_t *info, void *ctx);
bool try_recover_crash(crash_context_t *ctx);
void print_crash_report(crash_context_t *ctx);
void *get_crash_recovery_point(void);
void set_crash_recovery_point(void);
void print_crash_stats(void);

// ============ ELF PARSER ============
Elf64_Ehdr *parse_elf_header(void *base);
Elf64_Sym *find_elf_symbol(Elf64_Ehdr *ehdr, const char *name);
void **find_got_entry(Elf64_Ehdr *ehdr, const char *symbol);
void **find_plt_entry(Elf64_Ehdr *ehdr, const char *symbol);
int enumerate_symbols(Elf64_Ehdr *ehdr, void (*callback)(const char*, void*));
int enumerate_imports(Elf64_Ehdr *ehdr, void (*callback)(const char*, const char*));

// ============ LIBRARY MANAGER ============
missing_lib_t *find_or_create_stub_lib(const char *name);
void *get_lib_base(const char *name);
int enumerate_loaded_libs(void (*callback)(const char*, void*, size_t));
int replace_lib_reference(const char *old_name, const char *new_name);
void *create_virtual_lib(const char *name);
void dump_stubbed_libs(void);
int is_stub_handle(void *handle);
missing_lib_t *get_lib_by_handle(void *handle);
int is_known_missing_lib(const char *name);
int is_security_symbol(const char *name);

// ============ JNI BRIDGE ============
JNIEnv *get_jni_env(void);
int register_native_methods(JNIEnv *env, const char *class_name,
                             JNINativeMethod *methods, int count);
void *find_jni_method(const char *class_name, const char *method_name);
int auto_register_missing_jni(JNIEnv *env);

// ============ ANTI-DETECTION ============
int hide_root_traces(void);
int hide_debugger(void);
int spoof_build_fingerprint(void);
int bypass_emulator_detection(void);
int patch_security_checks(void);

// ============ UTILITY ============
void *safe_memcpy(void *dest, const void *src, size_t n);
int safe_strcpy(char *dest, const char *src, size_t size);
void hex_dump(const void *data, size_t size);
void print_backtrace(void);
void *find_symbol_near(void *addr, size_t range);
uint64_t get_timestamp_ns(void);
void sleep_ms(int ms);

// ============ INIT/SHUTDOWN ============
void __attribute__((constructor)) universal_stub_init(void);
void __attribute__((destructor)) universal_stub_fini(void);

#endif
