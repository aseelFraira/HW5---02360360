# Testing

## Directory Structure

```
tests/
├── run_tests.sh         # Automated test runner
├── allTests/
│   └── mytests/         # Test cases
└── README.md
```

Each test case consists of a pair of files:
- `<name>.in` — FuncC source code (compiler input)
- `<name>.out` — Expected output after executing the generated LLVM IR

## Running Tests

From the project root **or** from the `tests/` directory:

```bash
bash tests/run_tests.sh
```

The script will:
1. Build the compiler via `make`
2. For each `.in` file, compile it to LLVM IR and execute with `lli`
3. Compare actual output against the `.out` file
4. Print a pass/fail summary

### Prerequisites

- **Flex**, **Bison**, **g++** — for building the compiler
- **LLVM** (`lli`) — for executing the generated IR
- **dos2unix** *(optional)* — handles line-ending conversion on Windows test files

## Adding New Tests

1. Create a new subdirectory under `tests/allTests/`, or add to an existing one
2. Add `<name>.in` (source code) and `<name>.out` (expected output) file pairs
3. Run `bash tests/run_tests.sh` — new tests are picked up automatically

## Example

```bash
$ bash tests/run_tests.sh

=========================
     Running All Tests
=========================

▶ Directory: tests/allTests/hw5-tests/
  ✅ t1
  ✅ t2
  ✅ t3
  ✅ t4

▶ Directory: tests/allTests/mytests/
  ✅ myTest1
  ✅ myTest2
  ...

=========================
         Summary
=========================
✅ Passed: 37
❌ Failed: 0
=========================
```
