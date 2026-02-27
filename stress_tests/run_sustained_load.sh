#!/bin/bash

# Sustained Load Test
# Duration: 300 seconds (5 minutes)
# Connections: 5,000
# Threads: 10
# Purpose: Verify system stability under sustained load

echo "========================================="
echo "FRPC Sustained Load Test"
echo "========================================="
echo "Duration: 300 seconds (5 minutes)"
echo "Connections: 5,000"
echo "Threads: 10"
echo "Purpose: Verify system stability"
echo "========================================="
echo ""

# Create results directory if it doesn't exist
mkdir -p stress_tests/results

# Run wrk with sustained load
wrk -t 10 -c 5000 -d 300s \
    --latency \
    http://localhost:8080/rpc \
    | tee stress_tests/results/sustained_load_results.txt

echo ""
echo "Results saved to: stress_tests/results/sustained_load_results.txt"
echo ""

# Check for stability indicators
echo "========================================="
echo "Stability Check"
echo "========================================="

# Extract error count
ERRORS=$(grep "Socket errors:" stress_tests/results/sustained_load_results.txt | wc -l)
if [ $ERRORS -eq 0 ]; then
    echo "✅ PASS: No socket errors detected"
else
    echo "❌ FAIL: Socket errors detected"
    grep "Socket errors:" stress_tests/results/sustained_load_results.txt
fi

# Extract QPS
QPS=$(grep "Requests/sec:" stress_tests/results/sustained_load_results.txt | awk '{print $2}')
echo "Average QPS: $QPS"

# Extract latency stats
echo ""
echo "Latency Statistics:"
grep -A 4 "Latency Distribution" stress_tests/results/sustained_load_results.txt || \
    grep "Latency" stress_tests/results/sustained_load_results.txt | head -1

echo "========================================="
