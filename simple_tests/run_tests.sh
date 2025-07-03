#!/bin/bash

# Compile the project
make > /dev/null 2>&1

# Exit if compilation fails
if [ $? -ne 0 ]; then
    echo "❌ Compilation failed."
    exit 1
fi

TEST_DIRS=()
# get all sub-dirs of allTests
for dir in allTests/*/; do
    TEST_DIRS+=("$dir")
done

# Counters
PASS=0
FAIL=0

echo "========================="
echo "     Running All Tests"
echo "========================="

for DIR in "${TEST_DIRS[@]}"; do
    echo ""
    echo "▶ Directory: $DIR"
    
    dos2unix "$DIR"/*.in > /dev/null 2>&1
    dos2unix "$DIR"/*.out > /dev/null 2>&1

    for IN_FILE in "$DIR"/*.in; do
        BASENAME=$(basename "$IN_FILE" .in)
        OUT_FILE="$DIR/$BASENAME.out"
        LL_FILE="$DIR/$BASENAME.ll"
        RES_FILE="$DIR/$BASENAME.res"

        # Run program and save output to .res file
        ./hw5 < "$IN_FILE" 2>&1 > "$LL_FILE"
        lli "$LL_FILE" > "$RES_FILE" 2> /dev/null

        # Compare result
        if diff -q "$RES_FILE" "$OUT_FILE" > /dev/null; then
            echo -e "  ✅ $BASENAME"
            rm "$RES_FILE" "$LL_FILE" > /dev/null
            ((PASS++))
        else
            echo -e "  ❌ $BASENAME"
            ((FAIL++))
        fi
    done
done

make clean > /dev/null

# Final summary
echo ""
echo "========================="
echo "         Summary"
echo "========================="
echo -e "✅ Passed: $PASS"
echo -e "❌ Failed: $FAIL"
echo "========================="


