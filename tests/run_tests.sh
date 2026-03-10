#!/bin/bash

# Navigate to project root
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

# Parse arguments
SUITE=""
VERBOSE=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--verbose) VERBOSE=true; shift ;;
        *) SUITE="$1"; shift ;;
    esac
done

# Compile the project
echo "🔨 Building compiler..."
make > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed."
    exit 1
fi
echo ""

# Collect test directories
TEST_DIRS=()
if [ -n "$SUITE" ]; then
    if [ -d "tests/$SUITE" ]; then
        TEST_DIRS+=("tests/$SUITE")
    else
        echo "❌ Test suite '$SUITE' not found."
        echo "Available suites:"
        for dir in tests/*/; do
            name=$(basename "$dir")
            [ "$name" = "allTests" ] && continue
            count=$(ls "$dir"/*.in 2>/dev/null | wc -l)
            echo "  • $name ($count tests)"
        done
        exit 1
    fi
else
    for dir in tests/*/; do
        # Skip non-test directories
        name=$(basename "$dir")
        [ "$name" = "allTests" ] && continue
        [ -f "$dir"/*.in 2>/dev/null ] || ls "$dir"/*.in > /dev/null 2>&1 || continue
        TEST_DIRS+=("$dir")
    done
fi

# Counters
PASS=0
FAIL=0
TOTAL=0
FAILED_TESTS=()

echo "═══════════════════════════════════════"
echo "  FuncC Compiler — Test Runner"
echo "═══════════════════════════════════════"

for DIR in "${TEST_DIRS[@]}"; do
    SUITE_NAME=$(basename "$DIR")
    SUITE_PASS=0
    SUITE_FAIL=0

    echo ""
    echo "┌─ $SUITE_NAME"

    dos2unix "$DIR"/*.in > /dev/null 2>&1
    dos2unix "$DIR"/*.out > /dev/null 2>&1

    for IN_FILE in "$DIR"/*.in; do
        [ -f "$IN_FILE" ] || continue
        BASENAME=$(basename "$IN_FILE" .in)
        OUT_FILE="$DIR/$BASENAME.out"
        LL_FILE="$DIR/$BASENAME.ll"
        RES_FILE="$DIR/$BASENAME.res"

        ((TOTAL++))

        # Run compiler and execute LLVM IR
        ./hw5 < "$IN_FILE" > "$LL_FILE" 2>&1
        lli "$LL_FILE" > "$RES_FILE" 2> /dev/null

        # Compare result
        if diff -q "$RES_FILE" "$OUT_FILE" > /dev/null 2>&1; then
            echo "│  ✅ $BASENAME"
            rm -f "$RES_FILE" "$LL_FILE"
            ((PASS++))
            ((SUITE_PASS++))
        else
            echo "│  ❌ $BASENAME"
            ((FAIL++))
            ((SUITE_FAIL++))
            FAILED_TESTS+=("$SUITE_NAME/$BASENAME")

            if [ "$VERBOSE" = true ]; then
                echo "│     ┌── Expected:"
                sed 's/^/│     │  /' "$OUT_FILE"
                echo "│     ├── Got:"
                sed 's/^/│     │  /' "$RES_FILE"
                echo "│     └──"
            fi
            rm -f "$RES_FILE" "$LL_FILE"
        fi
    done

    echo "└─ $SUITE_PASS passed, $SUITE_FAIL failed"
done

make clean > /dev/null 2>&1

# Final summary
echo ""
echo "═══════════════════════════════════════"
echo "  Results: $PASS/$TOTAL passed"
echo "═══════════════════════════════════════"

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo ""
    echo "Failed tests:"
    for t in "${FAILED_TESTS[@]}"; do
        echo "  • $t"
    done
fi

echo ""

# Exit with proper code for CI
[ $FAIL -eq 0 ] && exit 0 || exit 1
