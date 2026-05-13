#include "universal_stub.h"

// Original function pointers
static void *(*orig_dlopen)(const char *, int) = NULL;
static void *(*orig_dlsym)(void *, const char *) = NULL;
static int (*orig_dlclose)(void *) = NULL;
static char *(*orig_dlerror)(void) = NULL;

// Thread-local error buffer
static __thread char g_dlerror_buf[512];

// ==================== DLOPEN INTERCEPTION ====================

void *universal_dlopen(const char *filename, int flags) {
    if (!orig_dlopen) {
        orig_dlopen = (void *(*)(const char *, int))dlsym(RTLD_NEXT, "dlopen");
    }

    if (!filename) {
        return orig_dlopen ? orig_dlopen(filename, flags) : NULL;
    }

    LOGI("[DLOPEN] Requested: %s (flags=0x%x)", filename, flags);

    // Try normal dlopen first
    void *handle = orig_dlopen ? orig_dlopen(filename, flags) : NULL;

    if (handle) {
        LOGI("[DLOPEN] Successfully loaded: %s (handle=%p)", filename, handle);
        return handle;
    }

    const char *err = dlerror();
    LOGW("[DLOPEN] Failed to load %s: %s", filename, err ? err : "unknown");

    // Check if it's a known missing library
    if (is_known_missing_lib(filename)) {
        LOGW("[DLOPEN] Known missing lib: %s - creating stub", filename);
        missing_lib_t *lib = find_or_create_stub_lib(filename);
        if (lib) {
            snprintf(g_dlerror_buf, sizeof(g_dlerror_buf),
                     "Stub library loaded: %s", filename);
            return lib->handle;
        }
    }

    // Check if it's any .so file
    if (strstr(filename, ".so") != NULL) {
        LOGW("[DLOPEN] Unknown missing lib: %s - creating stub", filename);
        missing_lib_t *lib = find_or_create_stub_lib(filename);
        if (lib) {
            snprintf(g_dlerror_buf, sizeof(g_dlerror_buf),
                     "Stub library loaded: %s", filename);
            return lib->handle;
        }
    }

    // Really not found
    snprintf(g_dlerror_buf, sizeof(g_dlerror_buf),
             "Cannot load %s: %s", filename, err ? err : "unknown");
    return NULL;
}

// ==================== DLSYM INTERCEPTION ====================

void *universal_dlsym(void *handle, const char *symbol) {
    if (!orig_dlsym) {
        orig_dlsym = (void *(*)(void *, const char *))dlsym(RTLD_NEXT, "dlsym");
    }

    if (!symbol) {
        return orig_dlsym ? orig_dlsym(handle, symbol) : NULL;
    }

    LOGI("[DLSYM] Requested: %s from handle %p", symbol, handle);

    // Check if handle is a stub handle
    missing_lib_t *lib = get_lib_by_handle(handle);
    if (lib && lib->is_stubbed) {
        LOGW("[DLSYM] Handle is stubbed (%s), generating symbol: %s",
             lib->name, symbol);
        return generate_symbol(symbol, infer_symbol_type(symbol));
    }

    // Try normal dlsym
    void *addr = orig_dlsym ? orig_dlsym(handle, symbol) : NULL;

    if (addr) {
        LOGD("[DLSYM] Found: %s at %p", symbol, addr);
        return addr;
    }

    // Symbol not found - generate stub
    LOGW("[DLSYM] Symbol not found: %s - generating stub", symbol);

    // Check if it's a security-related symbol
    if (is_security_symbol(symbol)) {
        LOGI("[DLSYM] Security symbol detected: %s", symbol);
    }

    return generate_symbol(symbol, infer_symbol_type(symbol));
}

// ==================== DLCLOSE INTERCEPTION ====================

int universal_dlclose(void *handle) {
    if (!orig_dlclose) {
        orig_dlclose = (int (*)(void *))dlsym(RTLD_NEXT, "dlclose");
    }

    // Check if it's a stub handle
    missing_lib_t *lib = get_lib_by_handle(handle);
    if (lib && lib->is_stubbed) {
        LOGI("[DLCLOSE] Stub handle closed: %s (refs=%d)",
             lib->name, lib->ref_count);
        lib->ref_count--;
        if (lib->ref_count <= 0) {
            lib->is_stubbed = 0;
        }
        return 0;
    }

    return orig_dlclose ? orig_dlclose(handle) : 0;
}

// ==================== DLERROR INTERCEPTION ====================

char *universal_dlerror(void) {
    if (g_dlerror_buf[0]) {
        // Return our custom error message and clear it for next call
        // (matches dlerror semantics: clears after read)
        static __thread char return_buf[512];
        strncpy(return_buf, g_dlerror_buf, sizeof(return_buf) - 1);
        return_buf[sizeof(return_buf) - 1] = '\0';
        g_dlerror_buf[0] = '\0';
        return return_buf;
    }

    if (!orig_dlerror) {
        orig_dlerror = (char *(*)(void))dlsym(RTLD_NEXT, "dlerror");
    }

    return orig_dlerror ? orig_dlerror() : NULL;
}

// ==================== EXPORTED INTERCEPTORS ====================

void *dlopen(const char *filename, int flags) {
    return universal_dlopen(filename, flags);
}

void *dlsym(void *handle, const char *symbol) {
    return universal_dlsym(handle, symbol);
}

int dlclose(void *handle) {
    return universal_dlclose(handle);
}

char *dlerror(void) {
    return universal_dlerror();
}

// ==================== UTILITY FUNCTIONS ====================

void *safe_memcpy(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;

    // Check for overlapping regions
    if ((src < dest && (char *)src + n > (char *)dest) ||
        (dest < src && (char *)dest + n > (char *)src)) {
        LOGW("[UTIL] Overlapping memcpy detected, using memmove");
        return memmove(dest, src, n);
    }

    return memcpy(dest, src, n);
}

int safe_strcpy(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return -1;

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < size - 1) ? src_len : size - 1;

    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return (src_len >= size) ? -1 : 0;
}

void hex_dump(const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;

    LOGI("[HEX] === Hex Dump (%zu bytes) ===", size);

    for (size_t i = 0; i < size; i += 16) {
        char hex[64] = {0};
        char ascii[17] = {0};

        for (size_t j = 0; j < 16 && i + j < size; j++) {
            snprintf(hex + j * 3, 4, "%02x ", bytes[i + j]);
            ascii[j] = isprint(bytes[i + j]) ? bytes[i + j] : '.';
        }

        LOGI("[HEX] %08zx: %-48s |%s|", i, hex, ascii);
    }
}

void print_backtrace(void) {
    void *buffer[MAX_BACKTRACE];
    int size = backtrace(buffer, MAX_BACKTRACE);

    LOGI("[BT] === Backtrace (%d frames) ===", size);
    for (int i = 0; i < size; i++) {
        Dl_info info;
        if (dladdr(buffer[i], &info)) {
            LOGI("[BT] #%02d: %p %s (%s+0x%lx)",
                 i, buffer[i],
                 info.dli_fname ? info.dli_fname : "???",
                 info.dli_sname ? info.dli_sname : "???",
                 (unsigned long)((char *)buffer[i] - (char *)info.dli_saddr));
        } else {
            LOGI("[BT] #%02d: %p ???", i, buffer[i]);
        }
    }
}

void *find_symbol_near(void *addr, size_t range) {
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_saddr) {
        return info.dli_saddr;
    }

    // Search nearby
    for (size_t offset = 0; offset < range; offset += 4) {
        void *test = (char *)addr - offset;
        if (dladdr(test, &info) && info.dli_saddr) {
            return info.dli_saddr;
        }
        test = (char *)addr + offset;
        if (dladdr(test, &info) && info.dli_saddr) {
            return info.dli_saddr;
        }
    }

    return NULL;
}

uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void sleep_ms(int ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000
    };
    nanosleep(&ts, NULL);
}

// ==================== ELF PARSER STUBS ====================

Elf64_Ehdr *parse_elf_header(void *base) {
    if (!base) return NULL;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        LOGE("[ELF] Invalid ELF magic");
        return NULL;
    }
    return ehdr;
}

Elf64_Sym *find_elf_symbol(Elf64_Ehdr *ehdr, const char *name) {
    if (!ehdr || !name) return NULL;

    // Parse section headers to find symtab and strtab
    Elf64_Shdr *shdr = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    char *shstrtab = (char *)ehdr + shdr[ehdr->e_shstrndx].sh_offset;

    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;
    size_t symtab_count = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf64_Sym *)((char *)ehdr + shdr[i].sh_offset);
            symtab_count = shdr[i].sh_size / sizeof(Elf64_Sym);
            // Link field points to string table section
            strtab = (char *)ehdr + shdr[shdr[i].sh_link].sh_offset;
            break;
        }
    }

    if (!symtab || !strtab) return NULL;

    for (size_t i = 0; i < symtab_count; i++) {
        if (symtab[i].st_name && strcmp(strtab + symtab[i].st_name, name) == 0) {
            return &symtab[i];
        }
    }

    return NULL;
}

void **find_got_entry(Elf64_Ehdr *ehdr, const char *symbol) {
    // Implemented in hook_framework.c hook_got_entry
    return NULL;
}

void **find_plt_entry(Elf64_Ehdr *ehdr, const char *symbol) {
    // Implemented in hook_framework.c hook_plt_entry
    return NULL;
}

int enumerate_symbols(Elf64_Ehdr *ehdr, void (*callback)(const char*, void*)) {
    if (!ehdr || !callback) return 0;

    Elf64_Shdr *shdr = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;
    size_t symtab_count = 0;
    int count = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            symtab = (Elf64_Sym *)((char *)ehdr + shdr[i].sh_offset);
            symtab_count = shdr[i].sh_size / sizeof(Elf64_Sym);
            strtab = (char *)ehdr + shdr[shdr[i].sh_link].sh_offset;
            break;
        }
    }

    if (!symtab || !strtab) return 0;

    for (size_t i = 0; i < symtab_count; i++) {
        if (symtab[i].st_name && symtab[i].st_value) {
            callback(strtab + symtab[i].st_name, (void *)symtab[i].st_value);
            count++;
        }
    }

    return count;
}

int enumerate_imports(Elf64_Ehdr *ehdr, void (*callback)(const char*, const char*)) {
    if (!ehdr || !callback) return 0;

    // Parse .dynsym and dynamic relocations to find imported symbols
    Elf64_Dyn *dyn = NULL;

    // Find dynamic section via program headers
    if (ehdr->e_phoff) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
        for (int i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_DYNAMIC) {
                dyn = (Elf64_Dyn *)((char *)ehdr + phdr[i].p_offset);
                break;
            }
        }
    }

    if (!dyn) return 0;

    // Parse dynamic entries
    Elf64_Sym *dynsym = NULL;
    char *dynstr = NULL;
    Elf64_Rela *rela = NULL;
    size_t rela_size = 0;
    size_t rela_entsize = sizeof(Elf64_Rela);
    Elf64_Rela *jmprel = NULL;
    size_t pltrelsz = 0;

    for (int i = 0; dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
            case DT_SYMTAB:
                dynsym = (Elf64_Sym *)((char *)ehdr + dyn[i].d_un.d_ptr);
                break;
            case DT_STRTAB:
                dynstr = (char *)ehdr + dyn[i].d_un.d_ptr;
                break;
            case DT_RELA:
                rela = (Elf64_Rela *)((char *)ehdr + dyn[i].d_un.d_ptr);
                break;
            case DT_RELASZ:
                rela_size = dyn[i].d_un.d_val;
                break;
            case DT_JMPREL:
                jmprel = (Elf64_Rela *)((char *)ehdr + dyn[i].d_un.d_ptr);
                break;
            case DT_PLTRELSZ:
                pltrelsz = dyn[i].d_un.d_val;
                break;
            case DT_RELAENT:
                rela_entsize = dyn[i].d_un.d_val;
                break;
        }
    }

    if (!dynsym || !dynstr) return 0;

    int count = 0;

    // Process GLOB_DAT and RELA relocations
    if (rela && rela_size > 0) {
        int num_rela = rela_size / rela_entsize;
        for (int i = 0; i < num_rela; i++) {
            int sym_idx = ELF64_R_SYM(rela[i].r_info);
            int type = ELF64_R_TYPE(rela[i].r_info);

            // GLOB_DAT = imported data, RELATIVE = not imports
            if (type == R_AARCH64_GLOB_DAT && sym_idx > 0) {
                const char *sym_name = dynstr + dynsym[sym_idx].st_name;
                if (sym_name && *sym_name) {
                    // Find which library provides this symbol
                    Dl_info info;
                    void *sym_addr = dlsym(RTLD_DEFAULT, sym_name);
                    const char *source = "unknown";
                    if (sym_addr && dladdr(sym_addr, &info) && info.dli_fname) {
                        source = info.dli_fname;
                    }
                    callback(sym_name, source);
                    count++;
                }
            }
        }
    }

    // Process PLT relocations (JUMP_SLOT = imported functions)
    if (jmprel && pltrelsz > 0) {
        int num_jmp = pltrelsz / rela_entsize;
        for (int i = 0; i < num_jmp; i++) {
            int sym_idx = ELF64_R_SYM(jmprel[i].r_info);
            int type = ELF64_R_TYPE(jmprel[i].r_info);

            if (type == R_AARCH64_JUMP_SLOT && sym_idx > 0) {
                const char *sym_name = dynstr + dynsym[sym_idx].st_name;
                if (sym_name && *sym_name) {
                    Dl_info info;
                    void *sym_addr = dlsym(RTLD_DEFAULT, sym_name);
                    const char *source = "unknown";
                    if (sym_addr && dladdr(sym_addr, &info) && info.dli_fname) {
                        source = info.dli_fname;
                    }
                    callback(sym_name, source);
                    count++;
                }
            }
        }
    }

    return count;
}
