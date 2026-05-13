#include "universal_stub.h"
#include <sys/mman.h>

static jit_allocator_t g_jit = {
    .count = 0,
    .used = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

void *jit_alloc(size_t size) {
    pthread_mutex_lock(&g_jit.mutex);

    if (size == 0 || size > SIZE_MAX - JIT_PAGE_SIZE + 1) {
        LOGE("[JIT] Invalid allocation size: %zu", size);
        pthread_mutex_unlock(&g_jit.mutex);
        return NULL;
    }

    // Round up to page size after checking for overflow.
    size_t alloc_size = ((size + JIT_PAGE_SIZE - 1) / JIT_PAGE_SIZE) * JIT_PAGE_SIZE;

    if (g_jit.count >= MAX_JIT_PAGES || alloc_size > SIZE_MAX - g_jit.used) {
        LOGE("[JIT] JIT allocation limit reached!");
        pthread_mutex_unlock(&g_jit.mutex);
        return NULL;
    }

    void *mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED) {
        LOGE("[JIT] mmap failed: %s", strerror(errno));
        pthread_mutex_unlock(&g_jit.mutex);
        return NULL;
    }

    // Fixed: track actual allocation size for proper cleanup
    g_jit.pages[g_jit.count] = mem;
    g_jit.page_sizes[g_jit.count] = alloc_size;
    g_jit.count++;
    g_jit.used += alloc_size;

    LOGD("[JIT] Allocated %zu bytes at %p (page %d/%d)",
         alloc_size, mem, g_jit.count, MAX_JIT_PAGES);

    pthread_mutex_unlock(&g_jit.mutex);
    return mem;
}

void jit_free_all(void) {
    pthread_mutex_lock(&g_jit.mutex);

    // Fixed: use tracked page_sizes for correct munmap instead of always JIT_PAGE_SIZE
    for (int i = 0; i < g_jit.count; i++) {
        if (g_jit.pages[i]) {
            munmap(g_jit.pages[i], g_jit.page_sizes[i]);
            g_jit.pages[i] = NULL;
            g_jit.page_sizes[i] = 0;
        }
    }

    g_jit.count = 0;
    g_jit.used = 0;

    LOGI("[JIT] Freed all JIT pages");
    pthread_mutex_unlock(&g_jit.mutex);
}

static int jit_set_page_permissions(void *addr, size_t size, int prot) {
    if (!addr || size == 0) return -1;

    long sys_page_size = sysconf(_SC_PAGESIZE);
    size_t page_size = (sys_page_size > 0) ? (size_t)sys_page_size : JIT_PAGE_SIZE;
    uintptr_t start = (uintptr_t)addr & ~(page_size - 1);
    uintptr_t end;

    if ((uintptr_t)addr > UINTPTR_MAX - size ||
        (uintptr_t)addr + size > UINTPTR_MAX - page_size + 1) {
        LOGE("[JIT] Permission range overflow");
        return -1;
    }

    end = ((uintptr_t)addr + size + page_size - 1) & ~(page_size - 1);

    if (mprotect((void *)start, end - start, prot) != 0) {
        LOGE("[JIT] mprotect failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int jit_protect_code(void *addr, size_t size) {
    if (!addr || size == 0) return -1;

    __builtin___clear_cache(addr, (char *)addr + size);
    return jit_set_page_permissions(addr, size, PROT_READ | PROT_EXEC);
}

void *jit_write_code(void *dest, const void *src, size_t size) {
    if (!dest || !src || size == 0) return NULL;

    if (jit_set_page_permissions(dest, size, PROT_READ | PROT_WRITE) != 0) {
        return NULL;
    }

    memcpy(dest, src, size);

    if (jit_protect_code(dest, size) != 0) {
        return NULL;
    }

    return dest;
}
