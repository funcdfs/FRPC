# FRPC Stress Testing

This directory contains stress testing scripts and configurations for validating the FRPC framework's performance under high load.

## Overview

Stress tests validate the following performance requirements:
- **Requirement 11.1**: System stability under concurrent load (10,000 connections)
- **Requirement 11.2**: QPS > 50,000
- **Requirement 11.3**: P99 latency < 10ms

## Prerequisites

### Install wrk (HTTP Load Testing Tool)

**macOS:**
```bash
brew install wrk
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install wrk
```

**Linux (from source):**
```bash
git clone https://github.com/wg/wrk.git
cd wrk
make
sudo cp wrk /usr/local/bin/
```

### Alternative: Install Apache JMeter

Download from: https://jmeter.apache.org/download_jmeter.cgi

## Test Scenarios

### 1. Basic Load Test
- **Duration**: 30 seconds
- **Connections**: 100
- **Threads**: 4
- **Purpose**: Baseline performance measurement

### 2. High Concurrency Test
- **Duration**: 60 seconds
- **Connections**: 10,000
- **Threads**: 10
- **Purpose**: Validate 10,000 concurrent connection requirement

### 3. Sustained Load Test
- **Duration**: 300 seconds (5 minutes)
- **Connections**: 5,000
- **Threads**: 10
- **Purpose**: Verify system stability under sustained load

### 4. Peak Load Test
- **Duration**: 60 seconds
- **Connections**: 20,000
- **Threads**: 20
- **Purpose**: Determine maximum capacity

## Running Stress Tests

### Step 1: Start FRPC Test Server

First, build and start the FRPC test server:

```bash
# Build the test server
cmake --build build --target stress_test_server

# Start the server (in one terminal)
./build/bin/stress_test_server
```

The server will listen on `localhost:8080` by default.

### Step 2: Run Stress Tests

#### Using wrk (Recommended)

**Basic Load Test:**
```bash
./stress_tests/run_basic_load.sh
```

**High Concurrency Test:**
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

**Run All Tests:**
```bash
./stress_tests/run_all_tests.sh
```

#### Using JMeter

```bash
jmeter -n -t stress_tests/frpc_stress_test.jmx -l results.jtl -e -o report/
```

### Step 3: Analyze Results

Results are saved to `stress_tests/results/` directory:
- `basic_load_results.txt`
- `high_concurrency_results.txt`
- `sustained_load_results.txt`
- `peak_load_results.txt`

## Understanding Results

### wrk Output Format

```
Running 30s test @ http://localhost:8080/rpc
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.50ms    1.20ms   15.00ms   85.00%
    Req/Sec    12.50k     1.50k    15.00k    70.00%
  1500000 requests in 30.00s, 150.00MB read
Requests/sec:  50000.00
Transfer/sec:      5.00MB
```

**Key Metrics:**
- **Latency Avg**: Average response time
- **Latency Max**: Maximum response time
- **Req/Sec**: Requests per second per thread
- **Requests/sec**: Total QPS (this is what we measure against > 50,000 target)
- **Transfer/sec**: Data throughput

### Success Criteria

✅ **PASS Criteria:**
- QPS > 50,000
- P99 latency < 10ms
- No connection errors
- System remains stable throughout test
- Memory usage remains stable (no leaks)

❌ **FAIL Criteria:**
- QPS < 50,000
- P99 latency >= 10ms
- Connection timeouts or errors
- Server crashes or hangs
- Memory leaks detected

### Calculating P99 Latency

wrk provides latency distribution. To get P99:
```bash
./stress_tests/run_with_latency_distribution.sh
```

This will output:
```
Latency Distribution
   50%    2.00ms
   75%    3.50ms
   90%    6.00ms
   99%    9.50ms  <-- This is P99
```

## Test Server Implementation

The stress test server (`stress_test_server.cpp`) implements a simple RPC service that:
1. Accepts connections on port 8080
2. Handles RPC requests using the FRPC framework
3. Responds with minimal processing to measure framework overhead
4. Collects performance metrics (QPS, latency, connection count)

## Monitoring During Tests

### System Resources

Monitor system resources during stress tests:

```bash
# CPU and memory usage
top -pid $(pgrep stress_test_server)

# Network connections
netstat -an | grep 8080 | wc -l

# File descriptors
lsof -p $(pgrep stress_test_server) | wc -l
```

### Server Metrics

The test server exposes metrics on port 8081:
```bash
curl http://localhost:8081/metrics
```

Output:
```json
{
  "qps": 52341,
  "avg_latency_ms": 2.5,
  "p99_latency_ms": 9.2,
  "active_connections": 9876,
  "total_requests": 1570230,
  "error_count": 0
}
```

## Troubleshooting

### Issue: "Too many open files"

Increase file descriptor limit:
```bash
ulimit -n 65536
```

Make permanent by editing `/etc/security/limits.conf`:
```
* soft nofile 65536
* hard nofile 65536
```

### Issue: "Cannot assign requested address"

Increase ephemeral port range:
```bash
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"
```

### Issue: "Connection refused"

Check if server is running:
```bash
ps aux | grep stress_test_server
netstat -an | grep 8080
```

### Issue: Low QPS

Possible causes:
1. Server not built in Release mode (use `-DCMAKE_BUILD_TYPE=Release`)
2. Insufficient system resources
3. Network bottleneck (use localhost for testing)
4. Too few wrk threads (increase with `-t` flag)

## Advanced Testing

### Custom Lua Scripts for wrk

Create custom request patterns:

```lua
-- custom_request.lua
wrk.method = "POST"
wrk.body   = '{"service":"echo","params":["hello"]}'
wrk.headers["Content-Type"] = "application/json"

request = function()
   return wrk.format(nil, "/rpc")
end

response = function(status, headers, body)
   if status ~= 200 then
      print("Error: " .. status)
   end
end
```

Run with:
```bash
wrk -t 10 -c 1000 -d 60s -s custom_request.lua http://localhost:8080/rpc
```

### Gradual Load Increase

Test how the system handles gradual load increase:

```bash
./stress_tests/run_gradual_load.sh
```

This script:
1. Starts with 100 connections
2. Increases by 1000 every 30 seconds
3. Monitors QPS and latency at each level
4. Stops when performance degrades

## Continuous Performance Testing

Integrate stress tests into CI/CD:

```bash
# Run quick smoke test
./stress_tests/run_smoke_test.sh

# Run full stress test suite
./stress_tests/run_all_tests.sh

# Check if results meet requirements
./stress_tests/validate_results.sh
```

## Results Archive

Historical test results are stored in `stress_tests/results/archive/` with timestamps:
- `YYYY-MM-DD_HH-MM-SS_basic_load.txt`
- `YYYY-MM-DD_HH-MM-SS_high_concurrency.txt`
- etc.

Compare results over time:
```bash
./stress_tests/compare_results.sh results/archive/2024-01-01_*.txt results/archive/2024-01-15_*.txt
```

## References

- [wrk Documentation](https://github.com/wg/wrk)
- [JMeter User Manual](https://jmeter.apache.org/usermanual/index.html)
- [FRPC Requirements](../.kiro/specs/frpc-framework/requirements.md)
- [FRPC Design Document](../.kiro/specs/frpc-framework/design.md)
