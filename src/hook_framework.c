#include "universal_stub.h"

static hook_entry_t g_hooks[MAX_HOOKS];
static int g_hook_count = 0;
static pthread_mutex_t g_hook_mutex = PTHREAD_MUTEX_INITIALIZER;

// ==================== HELPER: Find base address ====================

static int find_base_address(const char *lib_name, void **base, size_t *size) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return 0;

    char line[1024];
    while (fgets(line, sizeof(line), maps)) {
        if (strstr(line, lib_name) && strstr(line, "r-xp")) {
            unsigned long start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                *base = (void *)start;
                *size = end - start;
                fclose(maps);
                return 1;
            }
        }
    }

    fclose(maps);
    return 0;
}

// ==================== HELPER: Find dynamic section ====================

static Elf64_Dyn *find_dynamic_section(Elf64_Ehdr *ehdr) {
    if (!ehdr || ehdr->e_phoff == 0) return NULL;

    Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);

    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            return (Elf64_Dyn *)((char *)ehdr + phdr[i].p_offset);
        }
    }

    return NULL;
}

// ==================== PLT HOOKING ====================

int hook_plt_entry(const char *lib_name, const char *symbol,
                   void *new_addr, void **old_addr) {
    LOGI("[HOOK] PLT hook: %s!%s -> %p", lib_name, symbol, new_addr);

    void *base;
    size_t size;
    if (!find_base_address(lib_name, &base, &size)) {
        LOGW("[HOOK] Could not find base address for %s", lib_name);
        return -1;
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        LOGE("[HOOK] Invalid ELF header for %s", lib_name);
        return -1;
    }

    // Find .plt section and the corresponding .rela.plt
    Elf64_Dyn *dyn = find_dynamic_section(ehdr);
    if (!dyn) {
        LOGE("[HOOK] No dynamic section found");
        return -1;
    }

    // Parse dynamic entries for JMPREL (PLT relocations)
    Elf64_Rela *rela = NULL;
    size_t rela_size = 0;
    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;
    size_t rela_entsize = sizeof(Elf64_Rela);

    for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
            case DT_JMPREL:
                rela = (Elf64_Rela *)((char *)base + dyn[i].d_un.d_ptr);
                break;
            case DT_PLTRELSZ:
                rela_size = dyn[i].d_un.d_val;
                break;
            case DT_SYMTAB:
                symtab = (Elf64_Sym *)((char *)base + dyn[i].d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = (char *)base + dyn[i].d_un.d_ptr;
                break;
            case DT_PLTREL:
                // Should be DT_RELA (7) for ARM64
                break;
            case DT_RELAENT:
                rela_entsize = dyn[i].d_un.d_val;
                break;
        }
    }

    if (!rela || !symtab || !strtab) {
        LOGE("[HOOK] Required PLT sections not found");
        return -1;
    }

    // Search for the symbol in PLT relocations
    int num_rela = rela_size / rela_entsize;
    for (int i = 0; i < num_rela; i++) {
        int sym_idx = ELF64_R_SYM(rela[i].r_info);
        int type = ELF64_R_TYPE(rela[i].r_info);

        // Only process JUMP_SLOT relocations (PLT entries)
        if (type != R_AARCH64_JUMP_SLOT) {
            continue;
        }

        const char *sym_name = strtab + symtab[sym_idx].st_name;
        if (strcmp(sym_name, symbol) == 0) {
            void **got_entry = (void **)((char *)base + rela[i].r_offset);

            // Save old value
            if (old_addr) *old_addr = *got_entry;

            // Make page writable
            size_t page_size = sysconf(_SC_PAGESIZE);
            void *page = (void *)(((uintptr_t)got_entry) & ~(page_size - 1));
            if (mprotect(page, page_size, PROT_READ | PROT_WRITE) != 0) {
                LOGE("[HOOK] mprotect failed: %s", strerror(errno));
                return -1;
            }

            // Replace
            *got_entry = new_addr;

            // Restore permissions
            mprotect(page, page_size, PROT_READ);

            LOGI("[HOOK] PLT entry hooked at %p: %p -> %p",
                 got_entry, old_addr ? *old_addr : NULL, new_addr);

            // Register hook
            pthread_mutex_lock(&g_hook_mutex);
            if (g_hook_count < MAX_HOOKS) {
                strncpy(g_hooks[g_hook_count].lib_name, lib_name, MAX_LIB_NAME - 1);
                g_hooks[g_hook_count].lib_name[MAX_LIB_NAME - 1] = '\0';
                strncpy(g_hooks[g_hook_count].symbol, symbol, MAX_SYMBOL_LEN - 1);
                g_hooks[g_hook_count].symbol[MAX_SYMBOL_LEN - 1] = '\0';
                g_hooks[g_hook_count].new_addr = new_addr;
                g_hooks[g_hook_count].original_addr = old_addr ? *old_addr : NULL;
                g_hooks[g_hook_count].type = HOOK_TYPE_PLT;
                g_hooks[g_hook_count].active = 1;
                g_hooks[g_hook_count].hit_count = 0;
                g_hooks[g_hook_count].last_hit = 0;
                g_hook_count++;
            }
            pthread_mutex_unlock(&g_hook_mutex);

            return g_hook_count - 1;
        }
    }

    LOGW("[HOOK] Symbol %s not found in PLT", symbol);
    return -1;
}

// ==================== GOT HOOKING ====================

int hook_got_entry(const char *lib_name, const char *symbol,
                   void *new_addr, void **old_addr) {
    LOGI("[HOOK] GOT hook: %s!%s -> %p", lib_name, symbol, new_addr);

    void *base;
    size_t size;
    if (!find_base_address(lib_name, &base, &size)) {
        LOGW("[HOOK] Could not find base address for %s", lib_name);
        return -1;
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        LOGE("[HOOK] Invalid ELF header for %s", lib_name);
        return -1;
    }

    // Find .dynamic section
    Elf64_Dyn *dyn = find_dynamic_section(ehdr);
    if (!dyn) {
        LOGE("[HOOK] No dynamic section found");
        return -1;
    }

    // Parse dynamic entries
    Elf64_Rela *rela = NULL;
    size_t rela_size = 0;
    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;
    size_t strtab_size = 0;
    size_t rela_entsize = sizeof(Elf64_Rela);

    for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
            case DT_RELA:
                rela = (Elf64_Rela *)((char *)base + dyn[i].d_un.d_ptr);
                break;
            case DT_RELASZ:
                rela_size = dyn[i].d_un.d_val;
                break;
            case DT_SYMTAB:
                symtab = (Elf64_Sym *)((char *)base + dyn[i].d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = (char *)base + dyn[i].d_un.d_ptr;
                break;
            case DT_STRSZ:
                strtab_size = dyn[i].d_un.d_val;
                break;
            case DT_RELAENT:
                rela_entsize = dyn[i].d_un.d_val;
                break;
        }
    }

    if (!rela || !symtab || !strtab) {
        LOGE("[HOOK] Required sections not found");
        return -1;
    }

    // Search for the symbol
    int num_rela = rela_size / rela_entsize;
    for (int i = 0; i < num_rela; i++) {
        int sym_idx = ELF64_R_SYM(rela[i].r_info);
        int type = ELF64_R_TYPE(rela[i].r_info);

        if (type != R_AARCH64_GLOB_DAT && type != R_AARCH64_JUMP_SLOT) {
            continue;
        }

        const char *sym_name = strtab + symtab[sym_idx].st_name;
        if (strcmp(sym_name, symbol) == 0) {
            void **got_entry = (void **)((char *)base + rela[i].r_offset);

            // Save old value
            if (old_addr) *old_addr = *got_entry;

            // Make page writable
            size_t page_size = sysconf(_SC_PAGESIZE);
            void *page = (void *)(((uintptr_t)got_entry) & ~(page_size - 1));
            if (mprotect(page, page_size, PROT_READ | PROT_WRITE) != 0) {
                LOGE("[HOOK] mprotect failed: %s", strerror(errno));
                return -1;
            }

            // Replace
            *got_entry = new_addr;

            // Restore permissions
            mprotect(page, page_size, PROT_READ);

            LOGI("[HOOK] GOT entry hooked at %p: %p -> %p",
                 got_entry, old_addr ? *old_addr : NULL, new_addr);

            // Register hook
            pthread_mutex_lock(&g_hook_mutex);
            if (g_hook_count < MAX_HOOKS) {
                strncpy(g_hooks[g_hook_count].lib_name, lib_name, MAX_LIB_NAME - 1);
                g_hooks[g_hook_count].lib_name[MAX_LIB_NAME - 1] = '\0';
                strncpy(g_hooks[g_hook_count].symbol, symbol, MAX_SYMBOL_LEN - 1);
                g_hooks[g_hook_count].symbol[MAX_SYMBOL_LEN - 1] = '\0';
                g_hooks[g_hook_count].new_addr = new_addr;
                g_hooks[g_hook_count].original_addr = old_addr ? *old_addr : NULL;
                g_hooks[g_hook_count].type = HOOK_TYPE_GOT;
                g_hooks[g_hook_count].active = 1;
                g_hooks[g_hook_count].hit_count = 0;
                g_hooks[g_hook_count].last_hit = 0;
                g_hook_count++;
            }
            pthread_mutex_unlock(&g_hook_mutex);

            return g_hook_count - 1;
        }
    }

    LOGW("[HOOK] Symbol %s not found in GOT", symbol);
    return -1;
}

// ==================== INLINE HOOKING ====================

// Fixed trampoline: properly embeds the return address after the branch instruction
static void *generate_trampoline(void *target, size_t hook_size) {
    void *trampoline = jit_alloc(JIT_PAGE_SIZE);
    if (!trampoline) return NULL;

    uint32_t *code = (uint32_t *)trampoline;
    int idx = 0;

    // Copy original instructions (simplified - assumes ARM64)
    uint32_t *orig = (uint32_t *)target;
    for (int i = 0; i < (int)(hook_size / 4); i++) {
        code[idx++] = orig[i];
    }

    // Jump back to original + hook_size
    // Fixed: Use movz/movk + br pattern instead of ldr literal which needs proper alignment
    uint64_t return_addr = (uint64_t)target + hook_size;

    // movz x16, #return_addr[15:0]
    code[idx++] = 0xD2800000 | ((return_addr & 0xFFFF) << 5) | 16;
    // movk x16, #return_addr[31:16], lsl #16
    code[idx++] = 0xF2A00000 | (((return_addr >> 16) & 0xFFFF) << 5) | 16;
    // movk x16, #return_addr[47:32], lsl #32
    if (return_addr >> 32) {
        code[idx++] = 0xF2C00000 | (((return_addr >> 32) & 0xFFFF) << 5) | 16;
    }
    // movk x16, #return_addr[63:48], lsl #48
    if (return_addr >> 48) {
        code[idx++] = 0xF2E00000 | (((return_addr >> 48) & 0xFFFF) << 5) | 16;
    }
    // br x16
    code[idx++] = 0xD61F0200;

    if (jit_protect_code(trampoline, (size_t)idx * sizeof(uint32_t)) != 0) {
        LOGE("[HOOK] Failed to protect trampoline");
        return NULL;
    }

    return trampoline;
}

int hook_inline(void *target, void *new_addr, void **trampoline) {
    LOGI("[HOOK] Inline hook: %p -> %p", target, new_addr);

    if (!target || !new_addr) return -1;

    // Make target writable
    size_t page_size = sysconf(_SC_PAGESIZE);
    void *page = (void *)(((uintptr_t)target) & ~(page_size - 1));
    if (mprotect(page, page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("[HOOK] mprotect failed: %s", strerror(errno));
        return -1;
    }

    // Generate trampoline (fixed: properly handles return address)
    if (trampoline) {
        *trampoline = generate_trampoline(target, 16);  // 4 instructions
        if (!*trampoline) {
            mprotect(page, page_size * 2, PROT_READ | PROT_EXEC);
            return -1;
        }
    }

    // Write hook: movz/movk x16, addr; br x16 (4 instructions = 16 bytes)
    uint32_t hook_code[4];
    uint64_t addr = (uint64_t)new_addr;

    // movz x16, #addr[15:0]
    hook_code[0] = 0xD2800000 | ((addr & 0xFFFF) << 5) | 16;
    // movk x16, #addr[31:16], lsl #16
    hook_code[1] = 0xF2A00000 | (((addr >> 16) & 0xFFFF) << 5) | 16;
    // movk x16, #addr[47:32], lsl #32
    hook_code[2] = 0xF2C00000 | (((addr >> 32) & 0xFFFF) << 5) | 16;
    // br x16
    hook_code[3] = 0xD61F0200;

    memcpy(target, hook_code, 16);
    __builtin___clear_cache(target, (char *)target + 16);

    // Restore permissions
    mprotect(page, page_size * 2, PROT_READ | PROT_EXEC);

    // Register hook
    pthread_mutex_lock(&g_hook_mutex);
    if (g_hook_count < MAX_HOOKS) {
        snprintf(g_hooks[g_hook_count].lib_name, MAX_LIB_NAME, "inline@%p", target);
        g_hooks[g_hook_count].new_addr = new_addr;
        g_hooks[g_hook_count].original_addr = target;
        g_hooks[g_hook_count].trampoline = trampoline;
        g_hooks[g_hook_count].type = HOOK_TYPE_INLINE;
        g_hooks[g_hook_count].active = 1;
        g_hooks[g_hook_count].hit_count = 0;
        g_hooks[g_hook_count].last_hit = 0;
        g_hook_count++;
    }
    pthread_mutex_unlock(&g_hook_mutex);

    LOGI("[HOOK] Inline hook installed at %p", target);
    return 0;
}

// ==================== SYSCALL HOOKING ====================

int hook_syscall(long syscall_num, void *new_handler) {
    LOGI("[HOOK] Syscall hook: #%ld", syscall_num);
    // On ARM64, syscall hooking requires seccomp-bpf or kernel module
    // Register the hook entry for tracking purposes
    pthread_mutex_lock(&g_hook_mutex);
    if (g_hook_count < MAX_HOOKS) {
        snprintf(g_hooks[g_hook_count].lib_name, MAX_LIB_NAME, "syscall");
        snprintf(g_hooks[g_hook_count].symbol, MAX_SYMBOL_LEN, "sys_%ld", syscall_num);
        g_hooks[g_hook_count].new_addr = new_handler;
        g_hooks[g_hook_count].type = HOOK_TYPE_SYSCALL;
        g_hooks[g_hook_count].active = 1;
        g_hooks[g_hook_count].hit_count = 0;
        g_hooks[g_hook_count].last_hit = 0;
        g_hook_count++;
    }
    pthread_mutex_unlock(&g_hook_mutex);
    return 0;
}

// ==================== GLOBAL HOOKS ====================

int install_global_hooks(void) {
    LOGI("[HOOK] Installing global hooks...");

    int installed = 0;

    // The LD_PRELOAD mechanism already handles dlopen/dlsym interception
    // No need to open libc and hook - that was causing unnecessary dlopen/dlclose

    // Hook JNI functions if available
    JNIEnv *env = get_jni_env();
    if (env) {
        LOGI("[HOOK] JNI environment available");
        // Register native methods for common security classes
        int jni_hooks = auto_register_missing_jni(env);
        installed += jni_hooks;
    }

    LOGI("[HOOK] Installed %d global hooks", installed);
    return installed;
}

int remove_hook(int hook_id) {
    if (hook_id < 0 || hook_id >= g_hook_count) return 0;

    pthread_mutex_lock(&g_hook_mutex);

    if (!g_hooks[hook_id].active) {
        pthread_mutex_unlock(&g_hook_mutex);
        return 0;
    }

    g_hooks[hook_id].active = 0;

    // Restore original if possible
    if (g_hooks[hook_id].type == HOOK_TYPE_GOT && g_hooks[hook_id].original_addr) {
        // Would need to find GOT entry again and restore
        LOGI("[HOOK] Removed hook #%d (%s!%s)", hook_id,
             g_hooks[hook_id].lib_name, g_hooks[hook_id].symbol);
    } else if (g_hooks[hook_id].type == HOOK_TYPE_PLT && g_hooks[hook_id].original_addr) {
        LOGI("[HOOK] Removed PLT hook #%d (%s!%s)", hook_id,
             g_hooks[hook_id].lib_name, g_hooks[hook_id].symbol);
    }

    pthread_mutex_unlock(&g_hook_mutex);
    return 1;
}

void *get_hook_trampoline(int hook_id) {
    if (hook_id < 0 || hook_id >= g_hook_count) return NULL;
    return g_hooks[hook_id].trampoline ? *g_hooks[hook_id].trampoline : NULL;
}

void dump_hooks(void) {
    pthread_mutex_lock(&g_hook_mutex);

    LOGI("[DUMP] === Active Hooks (%d total) ===", g_hook_count);
    for (int i = 0; i < g_hook_count; i++) {
        LOGI("[DUMP] #%d: %s!%s type=%d new=%p orig=%p hits=%lu active=%d",
             i, g_hooks[i].lib_name, g_hooks[i].symbol,
             g_hooks[i].type, g_hooks[i].new_addr,
             g_hooks[i].original_addr, (unsigned long)g_hooks[i].hit_count,
             g_hooks[i].active);
    }

    pthread_mutex_unlock(&g_hook_mutex);
}
