/**
 * @file benchmark_memory_pool.cpp
 * @brief Performance benchmarks for memory pool allocation
 * 
 * This benchmark measures the performance of the custom memory pool allocator
 * compared to standard heap allocation. The goal is to verify that the memory
 * pool reduces heap allocations by at least 80% and improves allocation performance.
 * 
 * Performance target: 80% reduction in heap allocations
 * 
 * Requirements: 11.5, 11.6, 15.1
 */

#include <benchmark/benchmark.h>
#include "frpc/memory_pool.h"
#include <vector>
#include <memory>

using namespace frpc;

/**
 * @brief Benchmark memory pool allocation performance
 * 
 * Measures the time to allocate memory from the custom memory pool.
 * This should be significantly faster than heap allocation.
 */
static void BM_MemoryPoolAllocation(benchmark::State& state) {
    const size_t block_size = state.range(0);
    MemoryPool pool(block_size, 1000);  // Pre-allocate 1000 blocks
    
    for (auto _ : state) {
        void* ptr = pool.allocate(block_size);
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }
    
    state.SetLabel("Memory Pool");
}

/**
 * @brief Benchmark standard heap allocation performance (baseline)
 * 
 * Measures the time to allocate memory from the heap using malloc/free.
 * This serves as a baseline to compare against memory pool performance.
 */
static void BM_HeapAllocation(benchmark::State& state) {
    const size_t block_size = state.range(0);
    
    for (auto _ : state) {
        void* ptr = malloc(block_size);
        benchmark::DoNotOptimize(ptr);
        free(ptr);
    }
    
    state.SetLabel("Heap (malloc/free)");
}

/**
 * @brief Benchmark new/delete allocation performance (baseline)
 * 
 * Measures the time to allocate memory using C++ new/delete operators.
 */
static void BM_NewDeleteAllocation(benchmark::State& state) {
    const size_t block_size = state.range(0);
    
    for (auto _ : state) {
        uint8_t* ptr = new uint8_t[block_size];
        benchmark::DoNotOptimize(ptr);
        delete[] ptr;
    }
    
    state.SetLabel("C++ new/delete");
}

/**
 * @brief Benchmark memory pool with high contention
 * 
 * Measures allocation performance when multiple allocations are in flight.
 * This simulates real-world scenarios with many concurrent coroutines.
 */
static void BM_MemoryPoolHighContention(benchmark::State& state) {
    const size_t block_size = 256;  // Typical coroutine frame size
    const int num_allocations = state.range(0);
    MemoryPool pool(block_size, num_allocations * 2);
    
    for (auto _ : state) {
        std::vector<void*> ptrs;
        ptrs.reserve(num_allocations);
        
        // Allocate multiple blocks
        for (int i = 0; i < num_allocations; ++i) {
            ptrs.push_back(pool.allocate(block_size));
        }
        
        // Deallocate all blocks
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
        
        benchmark::DoNotOptimize(ptrs);
    }
    
    state.SetItemsProcessed(state.iterations() * num_allocations);
}

/**
 * @brief Benchmark heap allocation with high contention (baseline)
 * 
 * Baseline comparison for high contention scenario using heap allocation.
 */
static void BM_HeapHighContention(benchmark::State& state) {
    const size_t block_size = 256;
    const int num_allocations = state.range(0);
    
    for (auto _ : state) {
        std::vector<void*> ptrs;
        ptrs.reserve(num_allocations);
        
        for (int i = 0; i < num_allocations; ++i) {
            ptrs.push_back(malloc(block_size));
        }
        
        for (void* ptr : ptrs) {
            free(ptr);
        }
        
        benchmark::DoNotOptimize(ptrs);
    }
    
    state.SetItemsProcessed(state.iterations() * num_allocations);
}

/**
 * @brief Benchmark memory pool allocation pattern (allocate-use-free)
 * 
 * Simulates the typical usage pattern in coroutine scheduling:
 * allocate frame -> use coroutine -> free frame
 */
static void BM_MemoryPoolUsagePattern(benchmark::State& state) {
    const size_t block_size = 256;
    MemoryPool pool(block_size, 100);
    
    for (auto _ : state) {
        // Allocate
        void* ptr = pool.allocate(block_size);
        
        // Simulate usage (write to memory)
        std::memset(ptr, 0xAB, block_size);
        benchmark::DoNotOptimize(ptr);
        
        // Deallocate
        pool.deallocate(ptr);
    }
}

/**
 * @brief Benchmark memory pool fragmentation resistance
 * 
 * Tests allocation performance with random allocation/deallocation patterns
 * to verify the pool handles fragmentation well.
 */
static void BM_MemoryPoolFragmentation(benchmark::State& state) {
    const size_t block_size = 256;
    MemoryPool pool(block_size, 1000);
    std::vector<void*> ptrs;
    
    for (auto _ : state) {
        state.PauseTiming();
        
        // Create fragmentation by allocating and freeing in random order
        for (int i = 0; i < 100; ++i) {
            ptrs.push_back(pool.allocate(block_size));
        }
        
        // Free every other block
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i]);
        }
        ptrs.erase(std::remove_if(ptrs.begin(), ptrs.end(), 
                                  [](void* p) { return p == nullptr; }), 
                   ptrs.end());
        
        state.ResumeTiming();
        
        // Measure allocation in fragmented state
        void* ptr = pool.allocate(block_size);
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
        
        state.PauseTiming();
        
        // Clean up
        for (void* p : ptrs) {
            pool.deallocate(p);
        }
        ptrs.clear();
        
        state.ResumeTiming();
    }
}

/**
 * @brief Benchmark memory pool statistics overhead
 * 
 * Measures the overhead of collecting statistics from the memory pool.
 */
static void BM_MemoryPoolStatistics(benchmark::State& state) {
    const size_t block_size = 256;
    MemoryPool pool(block_size, 100);
    
    for (auto _ : state) {
        auto stats = pool.get_stats();
        benchmark::DoNotOptimize(stats);
    }
}

// Register benchmarks with different block sizes
// Small blocks (typical for small coroutine frames)
BENCHMARK(BM_MemoryPoolAllocation)->Arg(64);
BENCHMARK(BM_HeapAllocation)->Arg(64);
BENCHMARK(BM_NewDeleteAllocation)->Arg(64);

// Medium blocks (typical coroutine frame size)
BENCHMARK(BM_MemoryPoolAllocation)->Arg(256);
BENCHMARK(BM_HeapAllocation)->Arg(256);
BENCHMARK(BM_NewDeleteAllocation)->Arg(256);

// Large blocks
BENCHMARK(BM_MemoryPoolAllocation)->Arg(1024);
BENCHMARK(BM_HeapAllocation)->Arg(1024);
BENCHMARK(BM_NewDeleteAllocation)->Arg(1024);

// High contention scenarios
BENCHMARK(BM_MemoryPoolHighContention)->Range(10, 1000);
BENCHMARK(BM_HeapHighContention)->Range(10, 1000);

// Usage patterns
BENCHMARK(BM_MemoryPoolUsagePattern);
BENCHMARK(BM_MemoryPoolFragmentation);
BENCHMARK(BM_MemoryPoolStatistics);

BENCHMARK_MAIN();
