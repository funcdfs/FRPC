#!/bin/bash

# High Concurrency Test
# Duration: 60 seconds
# Connections: 10,000
# Threads: 10
# Purpose: Validate 10,000 concurrent connection requirement (Requirement 11.1)

echo "========================================="
echo "FRPC High Concurrency Test"
echo "========================================="
echo "Duration: 60 seconds"
echo "Connections: 10,000"
echo "Threads: 10"
echo "Target: QPS > 50,000, P99 < 10ms"
echo "========================================="
echo ""

# Check system limits
echo "Checking system limits..."
ulimit -n
if [ $(ulimit -n) -lt 65536 ]; then
    echo "WARNING: File descriptor limit is low. Increase with: ulimit -n 65536"
    echo "Attempting to increase limit..."
    ulimit -n 65536 2>/dev/null || echo "Failed to increase limit. Run as root or modify /etc/security/limits.conf"
fi
echo ""

# Create results directory if it doesn't exist
mkdir -p stress_tests/results

# Run wrk with high concurrency
wrk -t 10 -c 10000 -d 60s \
    --latency \
    http://localhost:8080/rpc \
    | tee stress_tests/results/high_concurrency_results.txt

echo ""
echo "Results saved to: stress_tests/results/high_concurrency_results.txt"
echo ""

# Parse and validate results
echo "========================================="
echo "Validation"
echo "========================================="

# Extract QPS
QPS=$(grep "Requests/sec:" stress_tests/results/high_concurrency_results.txt | awk '{print $2}')
echo "QPS: $QPS"

# Check if QPS meets requirement
if [ ! -z "$QPS" ]; then
    QPS_INT=$(echo $QPS | cut -d'.' -f1)
    if [ $QPS_INT -gt 50000 ]; then
        echo "✅ PASS: QPS > 50,000"
    else
        echo "❌ FAIL: QPS < 50,000"
    fi
fi

# Extract P99 latency (if available)
P99=$(grep "99%" stress_tests/results/high_concurrency_results.txt | awk '{print $2}')
if [ ! -z "$P99" ]; then
    echo "P99 Latency: $P99"
    # Convert to milliseconds and compare
    P99_MS=$(echo $P99 | sed 's/ms//')
    if [ ! -z "$P99_MS" ]; then
        P99_INT=$(echo $P99_MS | cut -d'.' -f1)
        if [ $P99_INT -lt 10 ]; then
            echo "✅ PASS: P99 latency < 10ms"
        else
            echo "❌ FAIL: P99 latency >= 10ms"
        fi
    fi
fi

echo "========================================="
