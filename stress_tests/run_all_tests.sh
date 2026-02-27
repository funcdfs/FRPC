#!/bin/bash

# Run All Stress Tests
# Executes all stress test scenarios in sequence

echo "========================================="
echo "FRPC Complete Stress Test Suite"
echo "========================================="
echo "This will run all stress tests in sequence:"
echo "1. Basic Load Test (30s)"
echo "2. High Concurrency Test (60s)"
echo "3. Sustained Load Test (300s)"
echo "4. Peak Load Test (60s)"
echo ""
echo "Total estimated time: ~8 minutes"
echo "========================================="
echo ""

# Check if server is running
if ! nc -z localhost 8080 2>/dev/null; then
    echo "❌ ERROR: FRPC server is not running on localhost:8080"
    echo "Please start the server first:"
    echo "  ./build/bin/stress_test_server"
    exit 1
fi

echo "✅ Server is running"
echo ""

# Create results directory
mkdir -p stress_tests/results
mkdir -p stress_tests/results/archive

# Timestamp for archiving
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")

# Run tests
echo "========================================="
echo "Test 1/4: Basic Load Test"
echo "========================================="
./stress_tests/run_basic_load.sh
echo ""
sleep 5

echo "========================================="
echo "Test 2/4: High Concurrency Test"
echo "========================================="
./stress_tests/run_high_concurrency.sh
echo ""
sleep 5

echo "========================================="
echo "Test 3/4: Sustained Load Test"
echo "========================================="
./stress_tests/run_sustained_load.sh
echo ""
sleep 5

echo "========================================="
echo "Test 4/4: Peak Load Test"
echo "========================================="
./stress_tests/run_peak_load.sh
echo ""

# Archive results
echo "========================================="
echo "Archiving Results"
echo "========================================="
cp stress_tests/results/basic_load_results.txt "stress_tests/results/archive/${TIMESTAMP}_basic_load.txt"
cp stress_tests/results/high_concurrency_results.txt "stress_tests/results/archive/${TIMESTAMP}_high_concurrency.txt"
cp stress_tests/results/sustained_load_results.txt "stress_tests/results/archive/${TIMESTAMP}_sustained_load.txt"
cp stress_tests/results/peak_load_results.txt "stress_tests/results/archive/${TIMESTAMP}_peak_load.txt"

echo "Results archived to: stress_tests/results/archive/${TIMESTAMP}_*.txt"
echo ""

# Generate summary report
echo "========================================="
echo "Test Summary"
echo "========================================="
echo ""

echo "Basic Load Test:"
grep "Requests/sec:" stress_tests/results/basic_load_results.txt
echo ""

echo "High Concurrency Test:"
grep "Requests/sec:" stress_tests/results/high_concurrency_results.txt
grep "99%" stress_tests/results/high_concurrency_results.txt || echo "P99 data not available"
echo ""

echo "Sustained Load Test:"
grep "Requests/sec:" stress_tests/results/sustained_load_results.txt
echo ""

echo "Peak Load Test:"
grep "Requests/sec:" stress_tests/results/peak_load_results.txt
echo ""

echo "========================================="
echo "All tests completed!"
echo "========================================="
