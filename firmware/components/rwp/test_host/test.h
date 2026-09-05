#pragma once
#include <stdio.h>
#include <stdlib.h>
static int g_checks = 0, g_fails = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fails++; \
    fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { g_checks++; long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { g_fails++; fprintf(stderr, "  FAIL %s:%d: %s == %s (%lld != %lld)\n", \
    __FILE__, __LINE__, #a, #b, _a, _b); } } while (0)
