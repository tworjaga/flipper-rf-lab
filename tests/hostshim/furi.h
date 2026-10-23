// Minimal host-test shim for the Flipper furi API.
// Lets the SDK-free algorithm sources (fixed_point.c, statistics.c,
// compression.c) be compiled and unit-tested with a desktop compiler.
// NOT used in the firmware build — only by tests/test_real.c.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef void FuriMutex;
typedef int  FuriStatus;
#define FuriStatusOk     0
#define FuriStatusError  (-1)

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)

#define furi_assert(x)  assert(x)
#define furi_check(x)   assert(x)

static inline uint32_t furi_get_tick(void) { return 0; }
