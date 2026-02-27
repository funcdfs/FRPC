# FRPC Performance Benchmarks

This directory contains performance benchmarks for the FRPC framework using Google Benchmark.

## Overview

The benchmarks measure the performance of critical components to ensure the framework meets its performance targets:

- **QPS > 50,000**: Queries per second
- **P99 latency < 10ms**: 99th percentile latency
- **Coroutine switching < 100ns**: Context switch overhead
- **80% reduction in heap allocations**: Memory pool efficiency

## Benchmark Suites

### 1. Serialization Benchmarks (`benchmark_serialization.cpp`)

Measures the performance of Request and Response serialization/deserialization:

- **BM_RequestSerialization**: Time to serialize Request objects
- **BM_RequestDeserialization**: Time to deserialize Request objects
- **BM_ResponseSerialization**: Time to serialize Response objects
- **BM_ResponseDeserialization**: Time to deserialize Response objects
- **BM_RequestRoundTrip**: Complete serialize + deserialize cycle

**Key Metrics:**
- Throughput (bytes/second)
- Latency per operation

### 2. Coroutine Benchmarks (`benchmark_coroutine.cpp`)

Measures the overhead of coroutine operations:

- **BM_CoroutineCreation**: Time to create a coroutine
- **BM_CoroutineSwitching**: **CRITICAL** - Suspend/resume overhead (target: < 100ns)
- **BM_MultipleCoroutineSwitches**: Multiple suspension points
- **BM_CoroutineMemoryAllocation**: Custom allocator overhead
- **BM_CoroutineDestruction**: Cleanup overhead
- **BM_NestedCoroutines**: Coroutine call chains
- **BM_CoroutineExceptionHandling**: Exception handling overhead

**Key Metrics:**
- Nanoseconds per switch
- Pass/Fail indicator for < 100ns target

### 3. Memory Pool Benchmarks (`benchmark_memory_pool.cpp`)

Compares memory pool performance against standard heap allocation:

- **BM_MemoryPoolAllocation**: Memory pool allocation time
- **BM_HeapAllocation**: Baseline heap (malloc/free) allocation
- **BM_NewDeleteAllocation**: Baseline C++ new/delete allocation
- **BM_MemoryPoolHighContention**: Multiple concurrent allocations
- **BM_HeapHighContention**: Baseline for high contention
- **BM_MemoryPoolUsagePattern**: Typical allocate-use-free pattern
- **BM_MemoryPoolFragmentation**: Fragmentation resistance
- **BM_MemoryPoolStatistics**: Statistics collection overhead

**Key Metrics:**
- Allocation time comparison (pool vs heap)
- Speedup factor
- Target: 80% reduction in heap allocations

### 4. Network I/O Benchmarks (`benchmark_network_io.cpp`)

Measures network layer performance:

- **BM_BufferPoolAllocation**: Buffer acquisition/release overhead
- **BM_SocketCreation**: Socket creation/destruction time
- **BM_LocalSocketThroughput**: Send/receive throughput
- **BM_EpollRegistration**: Event registration overhead
- **BM_ConnectionEstablishment**: TCP connection setup time
- **BM_MessageSendLatency**: Single message send latency
- **BM_ConcurrentConnections**: Scalability with multiple connections

**Key Metrics:**
- Throughput (bytes/second)
- Latency (nanoseconds)
- Scalability (connections handled)

## Building and Running

### Build All Benchmarks

```bash
cd build
cmake ..
make
```

### Run All Benchmarks

```bash
make run_benchmarks
```

### Run Individual Benchmarks

```bash
# Serialization benchmarks
./bin/benchmark_serialization

# Coroutine benchmarks
./bin/benchmark_coroutine

# Memory pool benchmarks
./bin/benchmark_memory_pool

# Network I/O benchmarks
./bin/benchmark_network_io
```

### Benchmark Options

Google Benchmark supports various command-line options:

```bash
# Run with specific filter
./bin/benchmark_coroutine --benchmark_filter=BM_CoroutineSwitching

# Run with specific number of iterations
./bin/benchmark_coroutine --benchmark_min_time=5.0

# Output results in JSON format
./bin/benchmark_coroutine --benchmark_format=json

# Output results to file
./bin/benchmark_coroutine --benchmark_out=results.json --benchmark_out_format=json

# Display time in different units
./bin/benchmark_coroutine --benchmark_time_unit=ns
```

## Interpreting Results

### Example Output

```
Run on (8 X 3000 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 256 KiB (x4)
  L3 Unified 8192 KiB (x1)
Load Average: 1.23, 1.45, 1.67
-------------------------------------------------------------------
Benchmark                         Time             CPU   Iterations
-------------------------------------------------------------------
BM_CoroutineSwitching            85 ns           85 ns      8234567  PASS (<100ns)
BM_RequestSerialization/64      123 ns          123 ns      5678901  520.325MB/s
BM_MemoryPoolAllocation/256      12 ns           12 ns     58901234
```

### Key Indicators

- **Time/CPU**: Average time per iteration
- **Iterations**: Number of times the benchmark ran
- **Throughput**: Bytes processed per second (for I/O benchmarks)
- **Labels**: Pass/Fail indicators for specific targets

### Performance Targets

✅ **PASS Criteria:**
- Coroutine switching < 100ns
- Memory pool faster than heap allocation
- Serialization throughput > 100 MB/s
- Network I/O supports high concurrency

❌ **FAIL Criteria:**
- Coroutine switching >= 100ns
- Memory pool slower than heap
- Serialization becomes bottleneck
- Network I/O doesn't scale

## Optimization Suggestions

Based on benchmark results, consider these optimizations:

### If Serialization is Slow:
- Use zero-copy techniques
- Optimize data structure layout
- Consider alternative serialization formats

### If Coroutine Switching is Slow:
- Review memory allocator implementation
- Minimize coroutine frame size
- Reduce suspension points

### If Memory Pool is Slow:
- Adjust block sizes
- Increase pre-allocated blocks
- Review free list implementation

### If Network I/O is Slow:
- Tune buffer sizes
- Optimize epoll configuration
- Review socket options (TCP_NODELAY, etc.)

## Continuous Performance Monitoring

Integrate benchmarks into CI/CD:

```bash
# Run benchmarks and save baseline
./bin/benchmark_coroutine --benchmark_out=baseline.json --benchmark_out_format=json

# Compare against baseline in future runs
./bin/benchmark_coroutine --benchmark_out=current.json --benchmark_out_format=json
# Use compare.py from Google Benchmark tools to compare
```

## Requirements Validation

These benchmarks validate the following requirements:

- **Requirement 11.1**: System stability under concurrent load
- **Requirement 11.2**: Acceptable throughput (QPS > 50,000)
- **Requirement 11.3**: P99 latency < 10ms
- **Requirement 11.5**: Coroutine switching < 100ns
- **Requirement 11.6**: 80% reduction in heap allocations
- **Requirement 15.1**: Detailed documentation and performance metrics

## Troubleshooting

### Benchmark Fails to Build

- Ensure Google Benchmark is installed or FetchContent can download it
- Check C++20 compiler support
- Verify all FRPC components are built

### Inconsistent Results

- Run benchmarks multiple times
- Disable CPU frequency scaling
- Close other applications
- Use `--benchmark_min_time` to run longer

### Performance Regression

- Compare against baseline results
- Check for recent code changes
- Profile with perf or valgrind
- Review compiler optimization flags

## References

- [Google Benchmark Documentation](https://github.com/google/benchmark)
- [FRPC Design Document](../.kiro/specs/frpc-framework/design.md)
- [FRPC Requirements](../.kiro/specs/frpc-framework/requirements.md)
