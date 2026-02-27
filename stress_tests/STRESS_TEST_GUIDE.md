# FRPC Stress Test Execution Guide

This guide provides step-by-step instructions for running stress tests to validate performance requirements.

## Prerequisites

### 1. Install wrk

**macOS:**
```bash
brew install wrk
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install wrk
```

**Verify installation:**
```bash
wrk --version
```

### 2. Build FRPC in Release Mode

**CRITICAL:** Stress tests must be run with Release builds for accurate performance measurement.

```bash
# Clean previous builds
rm -rf build

# Configure for Release mode
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build

# Build
cmake --build build -j$(nproc)

# Build stress test server
cmake --build build --target stress_test_server
```

### 3. Increase System Limits

```bash
# Increase file descriptor limit
ulimit -n 65536

# Verify
ulimit -n
```

To make permanent, edit `/etc/security/limits.conf`:
```
* soft nofile 65536
* hard nofile 65536
```

## Running Stress Tests

### Step 1: Start the Test Server

In one terminal:
```bash
./build/bin/stress_test_server
```

Expected output:
```
=========================================
FRPC Stress Test Server
=========================================
Port: 8080
=========================================

Server listening on port 8080
Press Ctrl+C to stop

[5s] QPS: 0 | Total: 0 | Active: 0 | Errors: 0
```

### Step 2: Run Stress Tests

In another terminal:

#### Option A: Run All Tests (Recommended)
```bash
./stress_tests/run_all_tests.sh
```

This will run all test scenarios in sequence (~8 minutes total).

#### Option B: Run Individual Tests

**Basic Load Test:**
```bash
./stress_tests/run_basic_load.sh
```

**High Concurrency Test (validates Requirements 11.1, 11.2, 11.3):**
```bash
./stress_tests/run_high_concurrency.sh
```

**Sustained Load Test:**
```bash
./stress_tests/run_sustained_load.sh
```

**Peak Load Test:**
```bash
./stress_tests/run_peak_load.sh
```

## Expected Results

### Requirement 11.1: System Stability (10,000 Concurrent Connections)

**Test:** High Concurrency Test  
**Command:** `./stress_tests/run_high_concurrency.sh`

**Expected Output:**
```
Running 60s test @ http://localhost:8080/rpc
  10 threads and 10000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.00ms    2.00ms   20.00ms   85.00%
    Req/Sec     6.00k     1.00k    10.00k    70.00%
  3600000 requests in 60.00s, 360.00MB read
Requests/sec:  60000.00
Transfer/sec:      6.00MB
```

**Success Criteria:**
- ✅ No socket errors
- ✅ Server remains stable throughout test
- ✅ All 10,000 connections handled successfully

### Requirement 11.2: QPS > 50,000

**Test:** High Concurrency Test  
**Command:** `./stress_tests/run_high_concurrency.sh`

**Expected Output:**
```
Requests/sec:  60000.00  <-- This value should be > 50,000
```

**Success Criteria:**
- ✅ QPS > 50,000

**Validation:**
The script automatically validates this:
```
QPS: 60000.00
✅ PASS: QPS > 50,000
```

### Requirement 11.3: P99 Latency < 10ms

**Test:** High Concurrency Test  
**Command:** `./stress_tests/run_high_concurrency.sh`

**Expected Output:**
```
Latency Distribution
   50%    3.00ms
   75%    5.00ms
   90%    7.00ms
   99%    9.00ms  <-- This value should be < 10ms
```

**Success Criteria:**
- ✅ P99 latency < 10ms

**Validation:**
The script automatically validates this:
```
P99 Latency: 9.00ms
✅ PASS: P99 latency < 10ms
```

## Interpreting Results

### Sample Output Analysis

```
Running 60s test @ http://localhost:8080/rpc
  10 threads and 10000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.23ms    2.15ms   18.45ms   84.23%
    Req/Sec     6.12k     1.23k    9.87k    68.45%
  Latency Distribution
     50%    4.50ms
     75%    6.20ms
     90%    8.10ms
     99%    9.50ms
  3672000 requests in 60.01s, 367.20MB read
Requests/sec:  61200.00
Transfer/sec:      6.12MB
```

**Analysis:**
- **QPS: 61,200** ✅ PASS (> 50,000)
- **P99 Latency: 9.50ms** ✅ PASS (< 10ms)
- **No errors** ✅ PASS
- **Stable throughout 60s** ✅ PASS

### Common Issues and Solutions

#### Issue 1: QPS < 50,000

**Possible Causes:**
1. Built in DEBUG mode instead of Release
2. Insufficient system resources
3. Network bottleneck
4. Too few wrk threads

**Solutions:**
```bash
# Rebuild in Release mode
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build

# Increase wrk threads
wrk -t 20 -c 10000 -d 60s --latency http://localhost:8080/rpc

# Check system resources
top -pid $(pgrep stress_test_server)
```

#### Issue 2: P99 Latency >= 10ms

**Possible Causes:**
1. System under heavy load
2. Network latency
3. Insufficient server resources
4. Memory allocation overhead

**Solutions:**
```bash
# Close other applications
# Use localhost (not remote host)
# Increase server resources
# Profile with perf

perf record -g ./build/bin/stress_test_server
perf report
```

#### Issue 3: Socket Errors

**Possible Causes:**
1. File descriptor limit too low
2. Port exhaustion
3. Server crashes

**Solutions:**
```bash
# Increase file descriptor limit
ulimit -n 65536

# Increase ephemeral port range
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"

# Check server logs
tail -f /var/log/syslog
```

#### Issue 4: Connection Timeouts

**Possible Causes:**
1. Server not running
2. Firewall blocking connections
3. Server overloaded

**Solutions:**
```bash
# Check if server is running
ps aux | grep stress_test_server
netstat -an | grep 8080

# Check firewall
sudo iptables -L

# Reduce concurrent connections
wrk -t 10 -c 5000 -d 60s --latency http://localhost:8080/rpc
```

## Monitoring During Tests

### Terminal 1: Server Output
```bash
./build/bin/stress_test_server
```

Watch for:
- QPS increasing
- Active connections count
- Error count (should be 0)

### Terminal 2: System Resources
```bash
# CPU and memory
top -pid $(pgrep stress_test_server)

# Network connections
watch -n 1 'netstat -an | grep 8080 | wc -l'

# File descriptors
watch -n 1 'lsof -p $(pgrep stress_test_server) | wc -l'
```

### Terminal 3: Run Tests
```bash
./stress_tests/run_all_tests.sh
```

## Results Documentation

### Saving Results

Results are automatically saved to:
- `stress_tests/results/basic_load_results.txt`
- `stress_tests/results/high_concurrency_results.txt`
- `stress_tests/results/sustained_load_results.txt`
- `stress_tests/results/peak_load_results.txt`

Archived with timestamps:
- `stress_tests/results/archive/YYYY-MM-DD_HH-MM-SS_*.txt`

### Creating Test Report

Create a summary report:

```bash
cat > stress_tests/results/TEST_REPORT.md << 'EOF'
# FRPC Stress Test Report

**Date:** $(date)
**Build:** Release
**Platform:** $(uname -a)

## Test Results

### High Concurrency Test (10,000 connections)
- **QPS:** [INSERT VALUE]
- **P99 Latency:** [INSERT VALUE]
- **Status:** [PASS/FAIL]

### Sustained Load Test (5 minutes)
- **Average QPS:** [INSERT VALUE]
- **Stability:** [PASS/FAIL]

### Peak Load Test (20,000 connections)
- **Peak QPS:** [INSERT VALUE]
- **Status:** [PASS/FAIL]

## Validation

- [ ] Requirement 11.1: System handles 10,000 concurrent connections
- [ ] Requirement 11.2: QPS > 50,000
- [ ] Requirement 11.3: P99 latency < 10ms

## Conclusion

[INSERT CONCLUSION]
EOF
```

## Next Steps

After successful stress testing:

1. **Document Results**
   - Save all test outputs
   - Create summary report
   - Archive results with timestamp

2. **Memory Leak Detection** (Task 16.6)
   ```bash
   # Rebuild with AddressSanitizer
   cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address" -S . -B build-asan
   cmake --build build-asan
   
   # Run tests
   ./build-asan/bin/stress_test_server
   ```

3. **Performance Optimization**
   - If QPS < 50,000: Profile and optimize
   - If P99 >= 10ms: Identify latency bottlenecks
   - If errors occur: Debug and fix

## Troubleshooting Checklist

Before reporting issues, verify:

- [ ] Built in Release mode (`-DCMAKE_BUILD_TYPE=Release`)
- [ ] File descriptor limit increased (`ulimit -n 65536`)
- [ ] Server is running (`ps aux | grep stress_test_server`)
- [ ] Port 8080 is accessible (`nc -z localhost 8080`)
- [ ] wrk is installed (`wrk --version`)
- [ ] No other applications using port 8080
- [ ] Sufficient system resources (CPU, memory, network)

## References

- [wrk Documentation](https://github.com/wg/wrk)
- [FRPC Requirements](../.kiro/specs/frpc-framework/requirements.md)
- [Benchmark Results](../benchmarks/BENCHMARK_RESULTS.md)
