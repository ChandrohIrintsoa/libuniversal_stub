#include "universal_stub.h"
#include <sys/mman.h>

static jit_allocator_t g_jit = {
    .count = 0,
    .used = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

void *jit_alloc(size_t size) {
    pthread_mutex_lock(&g_jit.mutex);

    // Round up to page size
    size_t alloc_size = ((size + JIT_PAGE_SIZE - 1) / JIT_PAGE_SIZE) * JIT_PAGE_SIZE;

    if (g_jit.count >= MAX_JIT_PAGES) {
        LOGE("[JIT] Max JIT pages reached!");
        pthread_mutex_unlock(&g_jit.mutex);
        return NULL;
    }

    void *mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE | PROT_EXEC,
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

void *jit_write_code(void *dest, const void *src, size_t size) {
    // Make the page temporarily writable
    size_t page_size = sysconf(_SC_PAGESIZE);
    void *page = (void *)(((uintptr_t)dest) & ~(page_size - 1));

    if (mprotect(page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("[JIT] mprotect failed: %s", strerror(errno));
        return NULL;
    }

    memcpy(dest, src, size);

    // Flush instruction cache (ARM64)
    __builtin___clear_cache(dest, (char *)dest + size);

    return dest;
}
