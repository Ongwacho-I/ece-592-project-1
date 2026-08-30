#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <x86intrin.h>


static inline uint64_t x86_tsc_start(void) {
    _mm_lfence();
    uint64_t t = __rdtsc();
    _mm_lfence();
    return t;
}
static inline uint64_t x86_tsc_stop(void) {
    unsigned aux;
    uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}
static inline uint64_t time_one_load(volatile const uint64_t *p) {
    uint64_t t0 = x86_tsc_start();
    uint64_t value = *p;
    uint64_t t1 = x86_tsc_stop();
    __asm__ __volatile__("" : "+r"(value) :: "memory");
    return t1-t0;
}

struct node {
    struct node *next;
};

static volatile struct node *sink_node;

static double chase(struct node *p, uint64_t steps) {
    uint64_t t0 = x86_tsc_start();
    for (uint64_t i = 0; i < steps; i++) {
        p = p->next;
    }
    uint64_t t1 = x86_tsc_start();
    sink_node = p;
    double latency_per_access = (double)(t1 - t0) / (double)steps;
    return latency_per_access;
}

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static void make_random_cycle(struct node *nodes, size_t n, uint32_t seed) {
    size_t *order = malloc(n * sizeof(*order));
    if (!order || n < 2) exit(1);
    for (size_t i = 0; i < n; i++) order[i] = i;
    uint32_t state = seed ? seed : 1u;
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)(xorshift32(&state) % (uint32_t)(i + 1));
        size_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
    for (size_t i = 0; i < n; i++)
        nodes[order[i]].next = &nodes[order[(i + 1) % n]];
    free(order);
}

int main(void) {
    for (uint32_t i = 120000; i <= 140000; i++) {
        struct node *addresses = malloc((size_t)i * sizeof(*addresses));
        if (!addresses) {
            fprintf(stderr, "malloc failed for size %u\n", i);
            break;
        }
        make_random_cycle(addresses, i, 0);
        double AAT = chase(addresses, 10000);
        printf("%llu\t%lf\n", (unsigned long long)i, AAT);
        free(addresses);
    }
    return 0;
}