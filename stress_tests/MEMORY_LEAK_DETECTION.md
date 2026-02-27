# FRPC Memory Leak Detection Guide

This guide provides instructions for detecting memory leaks in the FRPC framework using AddressSanitizer (ASan).

## Overview

Memory leak detection validates **Requirement 12.5**: The framework should not have memory leaks.

## Prerequisites

### Compiler Support

AddressSanitizer is supported by:
- GCC 4.8+
- Clang 3.1+
- Apple Clang (Xcode)

Verify compiler support:
```bash
gcc --version
clang --version
```

## Building with AddressSanitizer

### Method 1: CMake Configuration (Recommended)

```bash
# Clean previous builds
rm -rf build-asan

# Configure with AddressSanitizer
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
      -S . -B build-asan

# Build
cmake --build build-asan -j$(nproc)
```

### Method 2: Manual Compilation Flags

Add to CMakeLists.txt:
```cmake
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
```

## Running Tests with AddressSanitizer

### 1. Run Unit Tests

```bash
cd build-asan
ctest --output-on-failure
```

**Expected Output (No Leaks):**
```
Test project /path/to/frpc/build-asan
    Start 1: test_memory_pool
1/50 Test #1: test_memory_pool .................   Passed    0.12 sec
    Start 2: test_coroutine_scheduler
2/50 Test #2: test_coroutine_scheduler .........   Passed    0.15 sec
...
100% tests passed, 0 tests failed out of 50
```

**If Leaks Detected:**
```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 256 byte(s) in 1 object(s) allocated from:
    #0 0x7f8b9c in operator new(unsigned long) (/usr/lib/x86_64-linux-gnu/libasan.so.5+0x10c9c)
    #1 0x55d8a1 in MemoryPool::allocate(unsigned long) /path/to/frpc/src/memory_pool.cpp:45
    #2 0x55d9b2 in main /path/to/frpc/tests/test_memory_pool.cpp:23
    #3 0x7f8b8d in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x21b97)

SUMMARY: AddressSanitizer: 256 byte(s) leaked in 1 allocation(s).
```

### 2. Run Benchmark Tests

```bash
./build-asan/bin/benchmark_serialization
./build-asan/bin/benchmark_coroutine
./build-asan/bin/benchmark_memory_pool
```

**Note:** Benchmarks will run slower with ASan enabled (2-5x overhead).

### 3. Run Stress Test Server

```bash
# Start server with ASan
./build-asan/bin/stress_test_server

# In another terminal, run a short stress test
wrk -t 4 -c 100 -d 30s http://localhost:8080/rpc

# Stop server with Ctrl+C
# Check for leak reports in server output
```

### 4. Run Integration Tests

```bash
./build-asan/bin/integration_example
./build-asan/bin/complete_rpc_example
```

## Interpreting Results

### No Leaks Detected (Success)

```
=================================================================
==12345==LeakSanitizer: No leaks detected.
=================================================================
```

✅ **PASS**: No memory leaks found.

### Leaks Detected (Failure)

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 256 byte(s) in 1 object(s) allocated from:
    #0 0x7f8b9c in operator new(unsigned long)
    #1 0x55d8a1 in MemoryPool::allocate(unsigned long) memory_pool.cpp:45
    #2 0x55d9b2 in CoroutineScheduler::create_coroutine() coroutine_scheduler.cpp:78
    #3 0x55da23 in main test.cpp:23

Indirect leak of 128 byte(s) in 1 object(s) allocated from:
    #0 0x7f8b9c in operator new(unsigned long)
    #1 0x55db34 in std::vector::push_back() vector.h:456
    #2 0x55dc45 in ConnectionPool::get_connection() connection_pool.cpp:123

SUMMARY: AddressSanitizer: 384 byte(s) leaked in 2 allocation(s).
```

❌ **FAIL**: Memory leaks detected.

**Analysis:**
- **Direct leak**: Memory allocated but never freed
- **Indirect leak**: Memory referenced by leaked memory
- **Stack trace**: Shows where allocation occurred

### Common Leak Patterns

#### 1. Forgotten delete/free

```cpp
// BAD: Memory leak
void* ptr = malloc(256);
// ... use ptr ...
// Missing: free(ptr);

// GOOD: Proper cleanup
void* ptr = malloc(256);
// ... use ptr ...
free(ptr);
```

#### 2. Exception Safety

```cpp
// BAD: Leak on exception
void process() {
    auto* data = new Data();
    risky_operation();  // May throw
    delete data;  // Never reached if exception thrown
}

// GOOD: RAII
void process() {
    auto data = std::make_unique<Data>();
    risky_operation();  // data automatically cleaned up
}
```

#### 3. Circular References

```cpp
// BAD: Circular reference with shared_ptr
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;  // Circular reference!
};

// GOOD: Use weak_ptr for back references
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // Breaks cycle
};
```

## Fixing Memory Leaks

### Step 1: Identify the Leak

From the ASan output, identify:
1. **Allocation site**: Where memory was allocated
2. **Leak type**: Direct or indirect
3. **Size**: How much memory leaked

### Step 2: Trace the Code Path

Follow the stack trace to understand:
1. Why was memory allocated?
2. Where should it have been freed?
3. What prevented the cleanup?

### Step 3: Apply Fix

Common fixes:
1. Add missing `delete`/`free` calls
2. Use RAII (smart pointers, containers)
3. Fix exception safety
4. Break circular references
5. Ensure cleanup in destructors

### Step 4: Verify Fix

```bash
# Rebuild with fix
cmake --build build-asan

# Re-run tests
ctest --output-on-failure

# Verify no leaks
echo $?  # Should be 0
```

## Advanced Detection

### Leak Suppression File

For known false positives, create `lsan.supp`:

```
# Suppress known false positive in third-party library
leak:libthirdparty.so

# Suppress specific function
leak:ThirdPartyFunction
```

Use with:
```bash
export LSAN_OPTIONS=suppressions=lsan.supp
./build-asan/bin/test_program
```

### Additional ASan Options

```bash
# Detect use-after-free
export ASAN_OPTIONS=detect_stack_use_after_return=1

# Detect container overflow
export ASAN_OPTIONS=detect_container_overflow=1

# Verbose output
export ASAN_OPTIONS=verbosity=1

# Halt on error
export ASAN_OPTIONS=halt_on_error=1

# Combined options
export ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1:halt_on_error=0
```

### Memory Profiling

For detailed memory usage analysis:

```bash
# Install Valgrind
sudo apt-get install valgrind

# Run with Valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./build/bin/test_program

# Analyze results
cat valgrind-out.txt
```

## Continuous Leak Detection

### CI/CD Integration

Add to CI pipeline:

```yaml
# .github/workflows/memory-check.yml
name: Memory Leak Check

on: [push, pull_request]

jobs:
  memory-check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Build with ASan
        run: |
          cmake -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS="-fsanitize=address" \
                -S . -B build-asan
          cmake --build build-asan
      
      - name: Run Tests
        run: |
          cd build-asan
          ctest --output-on-failure
      
      - name: Check for Leaks
        run: |
          if grep -q "LeakSanitizer: detected memory leaks" build-asan/Testing/Temporary/LastTest.log; then
            echo "Memory leaks detected!"
            exit 1
          fi
```

### Automated Testing Script

```bash
#!/bin/bash
# test_memory_leaks.sh

set -e

echo "Building with AddressSanitizer..."
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fsanitize=address" \
      -S . -B build-asan
cmake --build build-asan

echo "Running tests..."
cd build-asan
ctest --output-on-failure 2>&1 | tee test_output.txt

echo "Checking for leaks..."
if grep -q "LeakSanitizer: detected memory leaks" test_output.txt; then
    echo "❌ FAIL: Memory leaks detected"
    exit 1
else
    echo "✅ PASS: No memory leaks detected"
    exit 0
fi
```

## Validation Checklist

Before marking Requirement 12.5 as complete:

- [ ] Built with AddressSanitizer enabled
- [ ] All unit tests pass without leaks
- [ ] All benchmark tests pass without leaks
- [ ] Stress test server runs without leaks
- [ ] Integration tests pass without leaks
- [ ] No direct leaks reported
- [ ] No indirect leaks reported
- [ ] Verified with both ASan and Valgrind
- [ ] Documented any suppressions (if needed)
- [ ] Added leak detection to CI/CD

## Expected Results

### Success Criteria

✅ **PASS** if:
- All tests complete successfully
- No "LeakSanitizer: detected memory leaks" messages
- No "SUMMARY: AddressSanitizer: X byte(s) leaked" messages
- Server runs for extended period without memory growth
- Valgrind reports "All heap blocks were freed -- no leaks are possible"

### Failure Criteria

❌ **FAIL** if:
- Any test reports memory leaks
- Server memory usage grows continuously
- Valgrind reports "definitely lost" or "indirectly lost" memory
- ASan reports direct or indirect leaks

## Troubleshooting

### Issue: False Positives

Some libraries may report false positives. Verify by:
1. Checking if leak is in third-party code
2. Reviewing library documentation
3. Creating suppression file if confirmed false positive

### Issue: ASan Crashes

If ASan causes crashes:
1. Reduce optimization level: `-O0` instead of `-O2`
2. Increase stack size: `ulimit -s unlimited`
3. Check for actual bugs that ASan is detecting

### Issue: Slow Performance

ASan adds 2-5x overhead. For faster testing:
1. Run subset of tests
2. Use Release build for performance tests
3. Use ASan only for leak detection, not benchmarks

## References

- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [LeakSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer)
- [Valgrind User Manual](https://valgrind.org/docs/manual/manual.html)
- [FRPC Requirements](../.kiro/specs/frpc-framework/requirements.md)
