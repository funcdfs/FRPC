#!/bin/bash

# Basic Load Test
# Duration: 30 seconds
# Connections: 100
# Threads: 4
# Purpose: Baseline performance measurement

echo "========================================="
echo "FRPC Basic Load Test"
echo "========================================="
echo "Duration: 30 seconds"
echo "Connections: 100"
echo "Threads: 4"
echo "========================================="
echo ""

# Create results directory if it doesn't exist
mkdir -p stress_tests/results

# Run wrk
wrk -t 4 -c 100 -d 30s \
    --latency \
    http://localhost:8080/rpc \
    | tee stress_tests/results/basic_load_results.txt

echo ""
echo "Results saved to: stress_tests/results/basic_load_results.txt"
