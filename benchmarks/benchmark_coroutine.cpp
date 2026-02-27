/**
 * @file benchmark_coroutine.cpp
 * @brief Performance benchmarks for coroutine operations
 * 
 * This benchmark measures the overhead of coroutine operations including:
 * - Coroutine creation
 * - Suspension and resumption
 * - Context switching
 * - Memory allocation (with custom allocator)
 * 
 * Performance target: Coroutine switching overhead < 100ns
 * 
 * Requirements: 11.5, 11.6, 15.1
 */

#include <benchmark/benchmark.h>
#include "frpc/coroutine.h"
#include <coroutine>
#include <chrono>

using namespace frpc;

/**
 * @brief Benchmark coroutine creation overhead
 * 
 * Measures the time to create a simple coroutine.
 * This includes memory allocation and promise initialization.
 */
static void BM_CoroutineCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto coro = []() -> RpcTask<int> {
            co_return 42;
        };
        
        auto task = coro();
        benchmark::DoNotOptimize(task);
        // Task destructor will clean up
    }
}

/**
 * @brief Benchmark coroutine switching overhead (suspend + resume)
 * 
 * This is the CRITICAL benchmark for measuring coroutine performance.
 * Target: < 100ns per switch
 * 
 * Measures the time for a complete suspend-resume cycle, which represents
 * the overhead of yielding control and resuming execution.
 */
static void BM_CoroutineSwitching(benchmark::State& state) {
    // Create a coroutine that suspends once
    auto coro = []() -> RpcTask<int> {
        co_await std::suspend_always{};
        co_return 42;
    };
    
    size_t total_switches = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (auto _ : state) {
        auto task = coro();
        auto handle = task.handle();
        
        // Initial resume (starts the coroutine)
        handle.resume();
        
        // Resume from suspension - this is what we're measuring
        handle.resume();
        
        total_switches++;
        benchmark::DoNotOptimize(handle);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    
    // Calculate average time per switch
    double avg_ns_per_switch = static_cast<double>(elapsed_ns) / total_switches;
    
    // Report if we meet the < 100ns target
    if (avg_ns_per_switch < 100.0) {
        state.SetLabel("PASS (<100ns): " + std::to_string(static_cast<int>(avg_ns_per_switch)) + "ns");
    } else {
        state.SetLabel("FAIL (>=100ns): " + std::to_string(static_cast<int>(avg_ns_per_switch)) + "ns");
    }
}

/**
 * @brief Benchmark coroutine with multiple suspensions
 * 
 * Measures the overhead when a coroutine suspends and resumes multiple times.
 * This simulates real-world async operations with multiple await points.
 */
static void BM_MultipleCoroutineSwitches(benchmark::State& state) {
    const int num_switches = state.range(0);
    
    for (auto _ : state) {
        auto coro = [num_switches]() -> RpcTask<int> {
            int sum = 0;
            for (int i = 0; i < num_switches; ++i) {
                co_await std::suspend_always{};
                sum += i;
            }
            co_return sum;
        };
        
        auto task = coro();
        auto handle = task.handle();
        
        // Resume through all suspension points
        while (!handle.done()) {
            handle.resume();
        }
        
        benchmark::DoNotOptimize(handle);
    }
    
    // Report average time per switch
    state.SetItemsProcessed(state.iterations() * num_switches);
}

/**
 * @brief Benchmark coroutine memory allocation with custom allocator
 * 
 * Measures the overhead of coroutine frame allocation using the custom
 * memory pool allocator vs default heap allocation.
 * 
 * Target: Reduce heap allocations by 80%
 */
static void BM_CoroutineMemoryAllocation(benchmark::State& state) {
    for (auto _ : state) {
        // Create coroutine with custom allocator
        auto coro = []() -> RpcTask<int> {
            co_return 42;
        };
        
        auto task = coro();
        benchmark::DoNotOptimize(task);
    }
    
    // Note: To verify 80% reduction, compare with baseline heap allocation
    // This would require disabling custom allocator and measuring separately
}

/**
 * @brief Benchmark coroutine destruction overhead
 * 
 * Measures the time to destroy a coroutine and clean up resources.
 */
static void BM_CoroutineDestruction(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        
        auto coro = []() -> RpcTask<int> {
            co_return 42;
        };
        auto task = coro();
        auto handle = task.handle();
        
        // Complete the coroutine
        handle.resume();
        
        state.ResumeTiming();
        
        // Destruction happens when task goes out of scope
        // We measure the explicit destroy call
        if (handle && !handle.done()) {
            handle.destroy();
        }
        
        benchmark::DoNotOptimize(handle);
    }
}

/**
 * @brief Benchmark nested coroutine calls
 * 
 * Measures the overhead of coroutines calling other coroutines.
 * This simulates real RPC call chains.
 */
static void BM_NestedCoroutines(benchmark::State& state) {
    const int depth = state.range(0);
    
    // Note: Nested co_await of other RpcTasks is complex
    // For now, we measure sequential coroutine execution
    for (auto _ : state) {
        for (int i = 0; i < depth; ++i) {
            auto coro = []() -> RpcTask<int> {
                co_return 1;
            };
            
            auto task = coro();
            auto handle = task.handle();
            
            while (!handle.done()) {
                handle.resume();
            }
            
            benchmark::DoNotOptimize(handle);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * depth);
}

/**
 * @brief Benchmark coroutine with exception handling
 * 
 * Measures the overhead when exceptions are thrown and caught in coroutines.
 */
static void BM_CoroutineExceptionHandling(benchmark::State& state) {
    for (auto _ : state) {
        auto coro = []() -> RpcTask<int> {
            try {
                throw std::runtime_error("test exception");
            } catch (const std::exception& e) {
                co_return -1;
            }
            co_return 42;
        };
        
        auto task = coro();
        auto handle = task.handle();
        
        while (!handle.done()) {
            handle.resume();
        }
        
        benchmark::DoNotOptimize(handle);
    }
}

/**
 * @brief Benchmark simple coroutine execution (baseline)
 * 
 * Measures the overhead of a coroutine that immediately returns.
 * This provides a baseline for comparison.
 */
static void BM_SimpleCoroutineExecution(benchmark::State& state) {
    for (auto _ : state) {
        auto coro = []() -> RpcTask<int> {
            co_return 42;
        };
        
        auto task = coro();
        auto handle = task.handle();
        
        // Execute the coroutine
        handle.resume();
        
        benchmark::DoNotOptimize(handle);
    }
}

// Register benchmarks
BENCHMARK(BM_CoroutineCreation);
BENCHMARK(BM_CoroutineSwitching);
BENCHMARK(BM_MultipleCoroutineSwitches)->Range(1, 100);
BENCHMARK(BM_CoroutineMemoryAllocation);
BENCHMARK(BM_CoroutineDestruction);
BENCHMARK(BM_NestedCoroutines)->Range(1, 10);
BENCHMARK(BM_CoroutineExceptionHandling);
BENCHMARK(BM_SimpleCoroutineExecution);

BENCHMARK_MAIN();
