#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#define BILLION 1000000000L

// Node structure for pointer chasing
struct Node {
    struct Node* next;
    uint64_t pad[7]; // Make node size exactly 64 bytes (one cache line)
};

// Helper to get time in nanoseconds
uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * BILLION + ts.tv_nsec;
}

// Fisher-Yates shuffle for random pointer chasing
void shuffle(size_t* array, size_t n) {
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            size_t t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

void run_pointer_chasing(size_t size_bytes, int random_mode) {
    size_t num_nodes = size_bytes / sizeof(struct Node);
    if (num_nodes < 2) return;

    struct Node* nodes = (struct Node*)malloc(num_nodes * sizeof(struct Node));
    if (!nodes) {
        printf("Failed to allocate %zu bytes\n", size_bytes);
        return;
    }

    size_t* indices = (size_t*)malloc(num_nodes * sizeof(size_t));
    if (!indices) {
        free(nodes);
        printf("Failed to allocate index array\n");
        return;
    }

    for (size_t i = 0; i < num_nodes; i++) {
        indices[i] = i;
    }

    if (random_mode) {
        shuffle(indices, num_nodes);
    }

    // Link the nodes
    for (size_t i = 0; i < num_nodes - 1; i++) {
        nodes[indices[i]].next = &nodes[indices[i + 1]];
    }
    // Complete the cycle
    nodes[indices[num_nodes - 1]].next = &nodes[indices[0]];

    // Warm up the cache
    struct Node* curr = &nodes[indices[0]];
    for (size_t i = 0; i < num_nodes * 2; i++) {
        curr = curr->next;
    }

    // Traverse and measure latency
    size_t traversals = 20000000; // 20M hops
    if (size_bytes > 32 * 1024 * 1024) {
        traversals = 5000000; // Reduce hops for very large memory to keep benchmark quick
    }

    uint64_t start = get_time_ns();
    for (size_t i = 0; i < traversals; i++) {
        curr = curr->next;
    }
    uint64_t end = get_time_ns();

    // Prevent compiler optimization by printing a dummy check
    if (curr == NULL) {
        printf("Dummy check failed\n");
    }

    double elapsed_sec = (double)(end - start) / BILLION;
    double latency_ns = (double)(end - start) / traversals;
    double throughput_mops = (double)traversals / elapsed_sec / 1000000.0;

    printf("%12zu | %s | %9.3f s | %10.3f ns | %10.3f Mops\n", 
           size_bytes, 
           random_mode ? "Random " : "Seq    ", 
           elapsed_sec, 
           latency_ns, 
           throughput_mops);

    free(indices);
    free(nodes);
}

void run_matrix_multiplication(int N) {
    printf("Running Matrix Multiplication (GEMM) of size %d x %d...\n", N, N);
    
    // Dynamically allocate to prevent stack overflow
    double* A = (double*)malloc(N * N * sizeof(double));
    double* B = (double*)malloc(N * N * sizeof(double));
    double* C = (double*)malloc(N * N * sizeof(double));
    
    if (!A || !B || !C) {
        printf("Failed to allocate matrices\n");
        if (A) free(A);
        if (B) free(B);
        if (C) free(C);
        return;
    }

    // Initialize matrices
    for (int i = 0; i < N * N; i++) {
        A[i] = (double)i * 0.0001;
        B[i] = (double)i * 0.0002;
        C[i] = 0.0;
    }

    uint64_t start = get_time_ns();
    // Cache-unfriendly stride access
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double sum = 0.0;
            for (int k = 0; k < N; k++) {
                sum += A[i * N + k] * B[k * N + j]; // B access has stride N
            }
            C[i * N + j] = sum;
        }
    }
    uint64_t end = get_time_ns();

    double elapsed_sec = (double)(end - start) / BILLION;
    double flops = 2.0 * N * N * N;
    double gflops = (flops / elapsed_sec) / 1000000000.0;

    printf("GEMM %d x %d completed in %.3f seconds (%.3f GFLOPS)\n", N, N, elapsed_sec, gflops);
    
    // Prevent optimization
    double dummy = C[0] + C[N * N - 1];
    if (dummy == -9999.9) printf("Dummy match!\n");

    free(A);
    free(B);
    free(C);
}

int main(int argc, char* argv[]) {
    srand(42);

    if (argc > 1 && strcmp(argv[1], "gemm") == 0) {
        int N = 512;
        if (argc > 2) N = atoi(argv[2]);
        run_matrix_multiplication(N);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "pointerchasing") == 0) {
        size_t size = 16 * 1024;
        int random_mode = 0;
        if (argc == 4) {
          size = atoi(argv[2]);
          random_mode = atoi(argv[3]);
        }
        printf("=== Pointer Chasing Cache/Memory Latency Benchmark (Single, Random: %d) ===\n", random_mode);
        printf("%12s | %s | %11s | %13s | %13s\n", "Size (Bytes)", "Access ", "Time (sec)", "Latency (ns)", "Throughput");
        run_pointer_chasing(size, random_mode);
        return 0;
    }

    printf("=== Pointer Chasing Cache/Memory Latency Benchmark ===\n");
    printf("%12s | %s | %11s | %13s | %13s\n", "Size (Bytes)", "Access ", "Time (sec)", "Latency (ns)", "Throughput");
    printf("-----------------------------------------------------------------------------\n");

    // Sweep sizes from 16 KB up to 64 MB
    size_t sizes[] = {
        16 * 1024,        // 16 KB (fits in L1d)
        64 * 1024,        // 64 KB (fits in L1d/L2)
        256 * 1024,       // 256 KB (fits in L2)
        1024 * 1024,      // 1 MB (fits in L3)
        4 * 1024 * 1024,  // 4 MB (exceeds individual L3 core slice or near L3 limit)
        16 * 1024 * 1024, // 16 MB (fits in large L3 or DRAM)
        64 * 1024 * 1024  // 64 MB (DRAM)
    };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < num_sizes; i++) {
        run_pointer_chasing(sizes[i], 0); // Sequential
        run_pointer_chasing(sizes[i], 1); // Random
    }

    return 0;
}
