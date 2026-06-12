// workload_cache_stress.c — matrix transpose exceeding L3
// gcc -O2 -o cache_stress workload_cache_stress.c -lm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Size chosen to exceed typical 8-32MB L3 caches
// 8192 * 8192 * 8 bytes = 512MB working set
#define N       8192
#define BLOCK   64      // Cache-line-aware blocking size (64 = typical line size)

static double A[N][N];
static double B[N][N];
static double C[N][N];

// ── Blocked matrix transpose: exposes cache line reuse within blocks ──
// Without blocking: column accesses in B cause a cache miss per element
// With BLOCK=64: each block fits in L1, but inter-block access still thrashes LLC
void transpose_blocked(double dst[N][N], double src[N][N]) {
    for (int ii = 0; ii < N; ii += BLOCK) {
        for (int jj = 0; jj < N; jj += BLOCK) {
            int imax = ii + BLOCK < N ? ii + BLOCK : N;
            int jmax = jj + BLOCK < N ? jj + BLOCK : N;
            for (int i = ii; i < imax; i++) {
                for (int j = jj; j < jmax; j++) {
                    dst[j][i] = src[i][j];
                }
            }
        }
    }
}

// ── Pointer-chase array: defeats hardware prefetcher entirely ─────────
// Random linked-list walk: each load depends on previous = serial miss chain
#define CHASE_SIZE  (1 << 24)   // 16M entries = 128MB (> L3 on most systems)
static size_t chase_array[CHASE_SIZE];

void init_pointer_chase(void) {
    // Fisher-Yates shuffle to create random permutation
    for (size_t i = 0; i < CHASE_SIZE; i++) chase_array[i] = i;
    srand(12345);
    for (size_t i = CHASE_SIZE - 1; i > 0; i--) {
        size_t j = (size_t)rand() % (i + 1);
        size_t tmp = chase_array[i];
        chase_array[i] = chase_array[j];
        chase_array[j] = tmp;
    }
}

volatile size_t run_pointer_chase(long steps) {
    volatile size_t idx = 0;
    for (long i = 0; i < steps; i++) {
        idx = chase_array[idx];   // Load-use dependency: serializes misses
    }
    return idx;
}

int main(void) {
    struct timespec t0, t1;

    // Initialize matrices with non-trivial values
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            A[i][j] = (double)(i * N + j) * 0.0001;
            B[i][j] = 0.0;
        }

    // ── Phase 1: Blocked transpose (tests L1/L2/L3 hierarchy) ─────────
    printf("Phase 1: Blocked Matrix Transpose (%dx%d, %.0fMB working set)...\n",
           N, N, (double)(2 * N * N * 8) / (1024 * 1024));
    clock_gettime(CLOCK_MONOTONIC, &t0);
    transpose_blocked(B, A);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ns1 = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    printf("  Transpose: %.3f sec | Throughput: %.2f GB/s\n",
           ns1 / 1e9, (2.0 * N * N * 8) / (ns1 / 1e9) / 1e9);

    // ── Phase 2: Pointer-chase (tests LLC miss latency serialized) ─────
    printf("Phase 2: Pointer-Chase Array (%zuMB, defeating prefetcher)...\n",
           CHASE_SIZE * sizeof(size_t) / (1024 * 1024));
    init_pointer_chase();
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile size_t result = run_pointer_chase(50000000L);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ns2 = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    printf("  Pointer-chase: %.3f sec | Avg latency: %.1f ns/op | result=%zu\n",
           ns2 / 1e9, (double)ns2 / 50000000.0, result);

    return 0;
}