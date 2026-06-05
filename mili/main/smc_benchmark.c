#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define BILLION 1000000000L

uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * BILLION + ts.tv_nsec;
}

int main(int argc, char* argv[]) {
    int iterations = 100000; // Default iterations
    if (argc > 1) {
        iterations = atoi(argv[1]);
    }

    printf("=== Self-Modifying Code (SMC) Benchmark ===\n");
    printf("Running %d iterations...\n", iterations);

    // 1. Allocate writable and executable memory
    size_t page_size = 4096;
    unsigned char* exec_mem = mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (exec_mem == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // 2. Initialize the machine code buffer depending on architecture
#if defined(__x86_64__)
    printf("Architecture detected: x86_64\n");
    // Opcode: B8 <4-byte immediate> (mov eax, imm32)
    // Opcode: C3 (ret)
    unsigned char template_code[] = {
        0xB8, 0x00, 0x00, 0x00, 0x00, // mov eax, 0
        0xC3                          // ret
    };
    size_t code_size = sizeof(template_code);
    memcpy(exec_mem, template_code, code_size);
#elif defined(__aarch64__)
    printf("Architecture detected: ARM64 (aarch64)\n");
    // Opcode: 52800000 | (imm16 << 5) (mov w0, #imm16)
    // Opcode: D65F03C0 (ret)
    uint32_t template_code[] = {
        0x52800000, // mov w0, #0
        0xD65F03C0  // ret
    };
    size_t code_size = sizeof(template_code);
    memcpy(exec_mem, template_code, code_size);
#else
#error "Unsupported architecture! This benchmark only supports x86_64 and aarch64."
#endif

    // Function pointer to call our dynamic memory block
    int (*dynamic_func)() = (int (*)())exec_mem;

    uint64_t start_time = get_time_ns();

    // 3. Execution loop
    for (int i = 0; i < iterations; i++) {
        // We write to the dynamic memory page, changing the return value to i (modulo immediate limit)
#if defined(__x86_64__)
        *(uint32_t*)(exec_mem + 1) = i;
#elif defined(__aarch64__)
        // ARM64 mov w0 instruction can only take 16-bit immediate in this format
        uint16_t imm = (uint16_t)(i & 0xFFFF);
        *(uint32_t*)(exec_mem) = 0x52800000 | (imm << 5) | 0; // mov w0, imm
#endif

        // 4. Force CPU instruction-cache invalidation / coherence
        // This is a compiler builtin that flushes data cache and invalidates instruction cache
        // for the specified memory range. On x86_64, this is practically a no-op due to hardware coherence,
        // but on ARM64 and under emulation it is strictly required and triggers cache traps.
        __builtin___clear_cache((char*)exec_mem, (char*)(exec_mem + code_size));

        // 5. Execute the modified block
        int ret_val = dynamic_func();

        // Simple validation check (print every 20% to show progress and verify correctness)
        if (i > 0 && i % (iterations / 5) == 0) {
            printf("Progress: %d/%d (Returned value matches: %s)\n", 
                   i, iterations, (ret_val == (i & 0xFFFF)) ? "YES" : "NO");
        }
    }

    uint64_t end_time = get_time_ns();
    double elapsed_sec = (double)(end_time - start_time) / BILLION;
    double mops = (double)iterations / elapsed_sec / 1000000.0;
    double ns_per_iteration = (double)(end_time - start_time) / iterations;

    printf("\nResults:\n");
    printf("Time elapsed    : %.6f seconds\n", elapsed_sec);
    printf("Throughput      : %.3f Million modifications/sec (MOps)\n", mops);
    printf("Latency per loop: %.2f ns\n", ns_per_iteration);

    // Clean up
    munmap(exec_mem, page_size);
    return 0;
}
