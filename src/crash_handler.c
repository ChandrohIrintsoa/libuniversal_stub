#include "universal_stub.h"
#include <setjmp.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif

static struct sigaction g_old_handlers[NSIG];
static volatile sig_atomic_t g_in_crash_handler = 0;
static __thread jmp_buf g_recovery_buf;
static __thread int g_has_recovery = 0;

// Crash statistics (Fixed: use CRASH_TYPE_MAX for array size to avoid OOB)
static struct {
    uint64_t total_crashes;
    uint64_t recovered_crashes;
    uint64_t fatal_crashes;
    uint64_t by_type[CRASH_TYPE_MAX];
} g_crash_stats = {0};

static pthread_mutex_t g_crash_mutex = PTHREAD_MUTEX_INITIALIZER;

static crash_type_t signal_to_crash_type(int sig) {
    switch (sig) {
        case SIGSEGV: return CRASH_TYPE_SIGSEGV;
        case SIGABRT: return CRASH_TYPE_SIGABRT;
        case SIGFPE:  return CRASH_TYPE_SIGFPE;
        case SIGBUS:  return CRASH_TYPE_SIGBUS;
        case SIGILL:  return CRASH_TYPE_SIGILL;
        case SIGTRAP: return CRASH_TYPE_SIGTRAP;
        default:      return CRASH_TYPE_NONE;
    }
}

static const char *crash_type_name(crash_type_t type) {
    switch (type) {
        case CRASH_TYPE_NONE:           return "NONE";
        case CRASH_TYPE_SIGSEGV:        return "SIGSEGV";
        case CRASH_TYPE_SIGABRT:        return "SIGABRT";
        case CRASH_TYPE_SIGFPE:         return "SIGFPE";
        case CRASH_TYPE_SIGBUS:         return "SIGBUS";
        case CRASH_TYPE_SIGILL:         return "SIGILL";
        case CRASH_TYPE_SIGTRAP:        return "SIGTRAP";
        case CRASH_TYPE_STACK_OVERFLOW:  return "STACK_OVERFLOW";
        case CRASH_TYPE_HEAP_CORRUPTION: return "HEAP_CORRUPTION";
        default:                        return "UNKNOWN";
    }
}

static void print_registers(ucontext_t *ctx) {
    if (!ctx) return;

    #ifdef __aarch64__
    mcontext_t *mctx = &ctx->uc_mcontext;
    LOGI("[CRASH] === Registers ===");
    for (int i = 0; i < 31; i++) {
        LOGI("[CRASH] X%02d: 0x%016llx", i, (unsigned long long)mctx->regs[i]);
    }
    LOGI("[CRASH] SP:  0x%016llx", (unsigned long long)mctx->sp);
    LOGI("[CRASH] PC:  0x%016llx", (unsigned long long)mctx->pc);
    LOGI("[CRASH] PSTATE: 0x%016llx", (unsigned long long)mctx->pstate);
    #elif defined(__x86_64__)
    LOGI("[CRASH] === Registers ===");
    LOGI("[CRASH] RAX: 0x%016llx", (unsigned long long)ctx->uc_mcontext.gregs[REG_RAX]);
    LOGI("[CRASH] RBX: 0x%016llx", (unsigned long long)ctx->uc_mcontext.gregs[REG_RBX]);
    LOGI("[CRASH] RIP: 0x%016llx", (unsigned long long)ctx->uc_mcontext.gregs[REG_RIP]);
    LOGI("[CRASH] RSP: 0x%016llx", (unsigned long long)ctx->uc_mcontext.gregs[REG_RSP]);
    #endif
}

static void print_backtrace_safe(void **buffer, int size) {
    LOGI("[CRASH] === Backtrace (%d frames) ===", size);
    for (int i = 0; i < size; i++) {
        Dl_info info;
        if (dladdr(buffer[i], &info)) {
            LOGI("[CRASH] #%02d: %p %s (%s+0x%lx)",
                 i, buffer[i],
                 info.dli_fname ? info.dli_fname : "???",
                 info.dli_sname ? info.dli_sname : "???",
                 (unsigned long)((char *)buffer[i] - (char *)info.dli_saddr));
        } else {
            LOGI("[CRASH] #%02d: %p ???", i, buffer[i]);
        }
    }
}

static void print_maps_near(void *addr) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return;

    LOGI("[CRASH] === Memory Maps Near %p ===", addr);
    char line[1024];
    while (fgets(line, sizeof(line), maps)) {
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            if ((void *)start <= addr && addr < (void *)end) {
                LOGI("[CRASH] %s", line);
            }
        }
    }
    fclose(maps);
}

static void print_crash_context(crash_context_t *ctx) {
    LOGI("[CRASH] ==================================================");
    LOGI("[CRASH] CRASH REPORT - %s", crash_type_name(ctx->type));
    LOGI("[CRASH] Signal: %d", ctx->signal);
    LOGI("[CRASH] Time: %s", ctime(&ctx->timestamp));

    if (ctx->info) {
        LOGI("[CRASH] Fault address: %p", ctx->info->si_addr);
        LOGI("[CRASH] Code: %d", ctx->info->si_code);
        LOGI("[CRASH] PID: %d", ctx->info->si_pid);
        LOGI("[CRASH] UID: %d", ctx->info->si_uid);
    }

    print_registers(ctx->context);
    print_backtrace_safe(ctx->backtrace, ctx->backtrace_size);

    if (ctx->info && ctx->info->si_addr) {
        print_maps_near(ctx->info->si_addr);
    }

    LOGI("[CRASH] Description: %s", ctx->description);
    LOGI("[CRASH] ==================================================");
}

static bool try_recover_sigsegv(crash_context_t *ctx) {
    if (!ctx->info || !ctx->info->si_addr) return false;

    void *fault_addr = ctx->info->si_addr;

    // Check if it's a null pointer dereference
    if (fault_addr == NULL || (uintptr_t)fault_addr < 0x1000) {
        LOGW("[CRASH] Null pointer dereference detected");
        // Can't really recover from this safely
        return false;
    }

    // Check if it's an unmapped region
    // Try to mmap a guard page
    size_t page_size = sysconf(_SC_PAGESIZE);
    void *page = (void *)(((uintptr_t)fault_addr) & ~(page_size - 1));

    void *mapped = mmap(page, page_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (mapped != MAP_FAILED) {
        LOGI("[CRASH] Recovered by mapping guard page at %p", page);
        memset(mapped, 0, page_size);
        return true;
    }

    return false;
}

static bool try_recover_sigabrt(crash_context_t *ctx) {
    // SIGABRT is usually intentional - check if it's from assert
    LOGW("[CRASH] SIGABRT detected - likely assertion failure");

    // Try to skip the abort and return
    #ifdef __aarch64__
    if (ctx->context) {
        mcontext_t *mctx = &ctx->context->uc_mcontext;
        // Skip the abort call by advancing PC
        mctx->pc += 4;
        LOGI("[CRASH] Skipped abort instruction");
        return true;
    }
    #endif

    return false;
}

static bool try_recover_sigfpe(crash_context_t *ctx) {
    LOGW("[CRASH] Floating point exception detected");

    #ifdef __aarch64__
    if (ctx->context) {
        mcontext_t *mctx = &ctx->context->uc_mcontext;
        // Skip the faulting instruction
        mctx->pc += 4;
        // Return 0 in x0
        mctx->regs[0] = 0;
        LOGI("[CRASH] Skipped FPE instruction, returning 0");
        return true;
    }
    #endif

    return false;
}

static bool try_recover_sigbus(crash_context_t *ctx) {
    LOGW("[CRASH] Bus error detected");

    #ifdef __aarch64__
    if (ctx->context) {
        mcontext_t *mctx = &ctx->context->uc_mcontext;
        mctx->pc += 4;
        LOGI("[CRASH] Skipped bus error instruction");
        return true;
    }
    #endif

    return false;
}

static bool try_recover_sigill(crash_context_t *ctx) {
    LOGW("[CRASH] Illegal instruction detected");

    #ifdef __aarch64__
    if (ctx->context) {
        mcontext_t *mctx = &ctx->context->uc_mcontext;
        // Skip the illegal instruction
        mctx->pc += 4;
        LOGI("[CRASH] Skipped illegal instruction");
        return true;
    }
    #endif

    return false;
}

bool try_recover_crash(crash_context_t *ctx) {
    if (!ctx) return false;

    // Bounds check before indexing into by_type array
    crash_type_t type = ctx->type;
    if (type < 0 || type >= CRASH_TYPE_MAX) {
        return false;
    }

    switch (type) {
        case CRASH_TYPE_SIGSEGV:
            return try_recover_sigsegv(ctx);
        case CRASH_TYPE_SIGABRT:
            return try_recover_sigabrt(ctx);
        case CRASH_TYPE_SIGFPE:
            return try_recover_sigfpe(ctx);
        case CRASH_TYPE_SIGBUS:
            return try_recover_sigbus(ctx);
        case CRASH_TYPE_SIGILL:
            return try_recover_sigill(ctx);
        default:
            return false;
    }
}

void crash_handler(int sig, siginfo_t *info, void *ucontext) {
    // Prevent recursive crashes
    if (g_in_crash_handler) {
        LOGE("[CRASH] Recursive crash detected! Aborting.");
        _exit(1);
    }
    g_in_crash_handler = 1;

    crash_context_t ctx = {0};
    ctx.type = signal_to_crash_type(sig);
    ctx.signal = sig;
    ctx.info = info;
    ctx.context = (ucontext_t *)ucontext;
    ctx.timestamp = time(NULL);

    // Get backtrace
    void *buffer[MAX_BACKTRACE];
    ctx.backtrace_size = backtrace(buffer, MAX_BACKTRACE);
    ctx.backtrace = buffer;

    snprintf(ctx.description, sizeof(ctx.description),
             "Signal %d (%s) at %p", sig, crash_type_name(ctx.type),
             info ? info->si_addr : NULL);

    print_crash_context(&ctx);

    // Bounds check before indexing into by_type array (fixed OOB bug)
    pthread_mutex_lock(&g_crash_mutex);
    g_crash_stats.total_crashes++;
    if (ctx.type >= 0 && ctx.type < CRASH_TYPE_MAX) {
        g_crash_stats.by_type[ctx.type]++;
    }
    pthread_mutex_unlock(&g_crash_mutex);

    // Try to recover
    ctx.recovered = try_recover_crash(&ctx);

    if (ctx.recovered) {
        pthread_mutex_lock(&g_crash_mutex);
        g_crash_stats.recovered_crashes++;
        pthread_mutex_unlock(&g_crash_mutex);

        LOGI("[CRASH] Successfully recovered from crash!");
        g_in_crash_handler = 0;
        return;
    }

    // Check if we have a recovery point (longjmp)
    if (g_has_recovery) {
        LOGW("[CRASH] Jumping to recovery point");
        g_has_recovery = 0;
        g_in_crash_handler = 0;
        longjmp(g_recovery_buf, sig);
        return;  // Never reached
    }

    // Fatal crash - call original handler
    pthread_mutex_lock(&g_crash_mutex);
    g_crash_stats.fatal_crashes++;
    pthread_mutex_unlock(&g_crash_mutex);

    LOGI("[CRASH] Fatal crash, calling original handler");
    g_in_crash_handler = 0;

    if (sig >= 0 && sig < NSIG &&
        g_old_handlers[sig].sa_handler != SIG_DFL &&
        g_old_handlers[sig].sa_handler != SIG_IGN) {
        if (g_old_handlers[sig].sa_flags & SA_SIGINFO) {
            g_old_handlers[sig].sa_sigaction(sig, info, ucontext);
        } else {
            g_old_handlers[sig].sa_handler(sig);
        }
    } else {
        // Default action
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

void install_crash_handlers(void) {
    LOGI("[CRASH] Installing crash handlers...");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
    sigemptyset(&sa.sa_mask);

    // Block all signals during handler
    sigfillset(&sa.sa_mask);

    int signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL, SIGTRAP};
    int installed = 0;

    for (int i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) {
        if (sigaction(signals[i], &sa, &g_old_handlers[signals[i]]) == 0) {
            installed++;
            LOGI("[CRASH] Installed handler for signal %d", signals[i]);
        } else {
            LOGW("[CRASH] Failed to install handler for signal %d: %s",
                 signals[i], strerror(errno));
        }
    }

    // Allocate alternate signal stack
    static char alt_stack[64 * 1024];
    stack_t ss = {
        .ss_sp = alt_stack,
        .ss_size = sizeof(alt_stack),
        .ss_flags = 0
    };
    if (sigaltstack(&ss, NULL) == 0) {
        LOGI("[CRASH] Alternate signal stack installed");
    }

    LOGI("[CRASH] Installed %d crash handlers", installed);
}

void uninstall_crash_handlers(void) {
    LOGI("[CRASH] Uninstalling crash handlers...");

    int signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL, SIGTRAP};

    for (int i = 0; i < sizeof(signals)/sizeof(signals[0]); i++) {
        sigaction(signals[i], &g_old_handlers[signals[i]], NULL);
    }

    LOGI("[CRASH] Crash handlers uninstalled");
}

void *get_crash_recovery_point(void) {
    return g_has_recovery ? (void *)g_recovery_buf : NULL;
}

void set_crash_recovery_point(void) {
    g_has_recovery = 1;
    if (setjmp(g_recovery_buf) != 0) {
        // Returned from longjmp
        LOGI("[CRASH] Recovered via longjmp");
        g_has_recovery = 0;
    }
}

void print_crash_stats(void) {
    pthread_mutex_lock(&g_crash_mutex);

    LOGI("[STATS] === Crash Statistics ===");
    LOGI("[STATS] Total crashes: %llu", (unsigned long long)g_crash_stats.total_crashes);
    LOGI("[STATS] Recovered: %llu", (unsigned long long)g_crash_stats.recovered_crashes);
    LOGI("[STATS] Fatal: %llu", (unsigned long long)g_crash_stats.fatal_crashes);
    LOGI("[STATS] By type:");
    for (int i = 1; i < CRASH_TYPE_MAX; i++) {
        if (g_crash_stats.by_type[i] > 0) {
            LOGI("[STATS]   %s: %llu", crash_type_name((crash_type_t)i),
                 (unsigned long long)g_crash_stats.by_type[i]);
        }
    }

    pthread_mutex_unlock(&g_crash_mutex);
}
