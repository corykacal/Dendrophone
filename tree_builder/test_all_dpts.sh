#!/bin/bash
# Test script to verify all .dpt files load correctly

echo "Testing all .dpt files in resources/test_graphs/"
echo "=============================================="

PASSED=0
FAILED=0
FAILED_FILES=()

for dpt_file in resources/test_graphs/*.dpt; do
    echo -n "Testing $(basename "$dpt_file")... "

    # Run Godot with the file (headless mode)
    timeout 5 godot --headless --path . -- "$dpt_file" --quit-after 1 > /tmp/godot_test.log 2>&1

    # Check if it loaded successfully
    if grep -q "Spawned.*nodes" /tmp/godot_test.log && ! grep -q "ERROR" /tmp/godot_test.log; then
        echo "✓ PASSED"
        ((PASSED++))
    else
        echo "✗ FAILED"
        ((FAILED++))
        FAILED_FILES+=("$dpt_file")
    fi
done

echo ""
echo "=============================================="
echo "Results: $PASSED passed, $FAILED failed"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo "Failed files:"
    for file in "${FAILED_FILES[@]}"; do
        echo "  - $file"
    done
    exit 1
fi

exit 0
