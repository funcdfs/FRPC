#!/bin/bash

# Peak Load Test
# Duration: 60 seconds
# Connections: 20,000
# Threads: 20
# Purpose: Determine maximum capacity

echo "========================================="
echo "FRPC Peak Load Test"
echo "========================================="
echo "Duration: 60 seconds"
echo "Connections: 20,000"
echo "Threads: 20"
echo "Purpose: Determine maximum capacity"
echo "========================================="
echo ""

# Check and increase system limits
echo "Checking system limits..."
ulimit -n
if [ $(ulimit -n) -lt 65536 ]; then
    echo "WARNING: File descriptor limit is low. Increasing..."
    ulimit -n 65536 2>/dev/null || echo "Failed to increase limit. Run as root."
fi
echo ""

# Create results directory if it doesn't exist
mkdir -p stress_tests/results

# Run wrk with peak load
wrk -t 20 -c 20000 -d 60s \
    --latency \
    http://localhost:8080/rpc \
    | tee stress_tests/results/peak_load_results.txt

echo ""
echo "Results saved to: stress_tests/results/peak_load_results.txt"
echo ""

# Analyze peak performance
echo "========================================="
echo "Peak Performance Analysis"
echo "========================================="

# Extract key metrics
QPS=$(grep "Requests/sec:" stress_tests/results/peak_load_results.txt | awk '{print $2}')
TRANSFER=$(grep "Transfer/sec:" stress_tests/results/peak_load_results.txt | awk '{print $2}')
TOTAL_REQUESTS=$(grep "requests in" stress_tests/results/peak_load_results.txt | awk '{print $1}')

echo "Peak QPS: $QPS"
echo "Transfer Rate: $TRANSFER"
echo "Total Requests: $TOTAL_REQUESTS"
echo ""

# Check for errors
if grep -q "Socket errors:" stress_tests/results/peak_load_results.txt; then
    echo "⚠️  WARNING: Socket errors detected at peak load"
    grep "Socket errors:" stress_tests/results/peak_load_results.txt
else
    echo "✅ No socket errors at peak load"
fi

echo "========================================="
