// core/circular_buffer.c
// Single-writer (ISR) / single-reader (thread) ring buffer.
//
// FIX (major): count is modified by both ISR (write path) and thread (read
//   path). volatile alone does not prevent a torn read-modify-write on
//   Cortex-M.  Use __atomic builtins with RELAXED ordering — sufficient for
//   single-core M4 and correctly documents the intent for any future SMP port.
//
// FIX (minor): memory_pool_free() is a documented no-op; renamed + assertion
//   added so misuse is caught at runtime during development.

#include "flipper_rf_lab.h"

// ============================================================================
// CIRCULAR BUFFER
// ============================================================================

void circular_buffer_init(CircularBuffer_t* cb, uint8_t* buffer, uint32_t size) {
    furi_assert(cb);
    furi_assert(buffer);
    furi_assert(size > 0);
    cb->buffer = buffer;
    cb->size   = size;
    cb->head   = 0;
    cb->tail   = 0;
    cb->count  = 0;
    cb->mutex  = NULL;
}

// Safe for ISR context (single writer assumed).
// FIX: use __atomic_fetch_add to make the count increment non-torn.
bool circular_buffer_write(CircularBuffer_t* cb, uint8_t data) {
    uint32_t current = __atomic_load_n(&cb->count, __ATOMIC_RELAXED);
    if(current >= cb->size) return false;
    cb->buffer[cb->head] = data;
    cb->head = (cb->head + 1) % cb->size;
    __atomic_fetch_add(&cb->count, 1, __ATOMIC_RELEASE);
    return true;
}

// Call from thread context only.
// FIX: use __atomic_fetch_sub to match the ISR-side atomic increment.
bool circular_buffer_read(CircularBuffer_t* cb, uint8_t* data) {
    uint32_t current = __atomic_load_n(&cb->count, __ATOMIC_ACQUIRE);
    if(current == 0) return false;
    *data = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % cb->size;
    __atomic_fetch_sub(&cb->count, 1, __ATOMIC_RELAXED);
    return true;
}

uint32_t circular_buffer_count(CircularBuffer_t* cb) {
    return __atomic_load_n(&cb->count, __ATOMIC_RELAXED);
}

void circular_buffer_clear(CircularBuffer_t* cb) {
    // Must only be called when no concurrent ISR/thread access is possible.
    cb->head  = 0;
    cb->tail  = 0;
    __atomic_store_n(&cb->count, 0, __ATOMIC_RELAXED);
}

// ============================================================================
// MEMORY POOL — bump allocator, init-time allocations only
// ============================================================================

#define MEMORY_POOL_SIZE (16 * 1024)

static uint8_t  pool_arena[MEMORY_POOL_SIZE] __attribute__((aligned(8)));
static uint32_t pool_offset      = 0;
static bool     pool_initialized = false;

bool memory_pools_init(void) {
    pool_offset      = 0;
    pool_initialized = true;
    return true;
}

void* memory_pool_alloc(uint32_t size) {
    furi_assert(pool_initialized);
    size = (size + 7) & ~7U;
    if(pool_offset + size > MEMORY_POOL_SIZE) {
        FURI_LOG_E("MemPool", "Pool exhausted: requested %lu, available %lu",
                   size, (uint32_t)(MEMORY_POOL_SIZE - pool_offset));
        return NULL;
    }
    void* ptr = &pool_arena[pool_offset];
    pool_offset += size;
    return ptr;
}

// FIX (minor): bump allocator cannot reclaim memory; calling free() is a
//   programmer error.  Assert in debug builds; no-op silently in release.
void memory_pool_free(void* ptr) {
    UNUSED(ptr);
    // Bump allocator: memory is never individually freed.
    // If you are seeing this assert, restructure the calling code to use
    // the allocator only for permanent init-time objects.
    furi_assert(false && "memory_pool_free: bump allocator does not reclaim memory");
}
