#!/bin/sh
# Host-based unit tests for Treadmill_server (no ESP-IDF needed).
# Used by CI (.github/workflows/host-tests.yml) and by hand.
# Exits non-zero on the first failing suite.
set -e
cd "$(dirname "$0")"

for t in test_ir_safety_logic test_safety_key_logic test_motor_calibration \
         test_ir_zone_filter test_ir_sample_age test_motor_feedback \
         test_settings test_blackbox test_fault_injection; do
    echo "=== $t ==="
    g++ -std=c++17 -Wall -I../main -o "/tmp/$t" "$t.cpp"
    "/tmp/$t" | tail -1
done
echo "ALL SERVER HOST SUITES PASSED"
