# Testing

## Test Suites

Tests are organized by feature area, each in its own directory:

| Suite | Tests | Description |
|---|---|---|
| `arithmetic/` | 5 | Integer ops, byte mixing, precedence, overflow, type promotion |
| `arrays/` | 7 | Init, iterate, sort, search, matrix multiply, bounds, statistics |
| `boolean_logic/` | 3 | AND/OR/NOT, short-circuit evaluation, complex expressions |
| `control_flow/` | 3 | While loops, nested loops, pattern printing, countdown |
| `edge_cases/` | 5 | Division by zero, out-of-bounds access (byte & int arrays) |
| `functions/` | 5 | Recursion, return values, helper functions, multiple params |
| `minimal/` | 2 | Hello world, single variable print |
| `relational/` | 1 | All comparison operators (`<`,`>`,`<=`,`>=`,`==`,`!=`) |
| `scoping/` | 1 | Nested block scoping |

Each test case consists of a pair of files:
- `<name>.in` — FuncC source code (compiler input)
- `<name>.out` — Expected output after executing the generated LLVM IR

## Running Tests

### Run all tests

```bash
bash tests/run_tests.sh
```

### Run a specific suite

```bash
bash tests/run_tests.sh arrays
bash tests/run_tests.sh edge_cases
```

### Verbose mode (show diffs on failure)

```bash
bash tests/run_tests.sh -v
bash tests/run_tests.sh arrays -v
```

### Prerequisites

- **Flex**, **Bison**, **g++** — for building the compiler
- **LLVM** (`lli`) — for executing the generated IR
- **dos2unix** *(optional)* — handles line-ending conversion

## Adding New Tests

1. Pick the right category directory (or create a new one under `tests/`)
2. Add a `<descriptive_name>.in` and `<descriptive_name>.out` pair
3. Run `bash tests/run_tests.sh` — new tests are picked up automatically

**Naming convention:** Use `snake_case` names that describe *what* the test verifies, e.g. `byte_overflow.in`, `nested_blocks.in`, `division_by_zero.in`.

## Example Output

```
🔨 Building compiler...

═══════════════════════════════════════
  FuncC Compiler — Test Runner
═══════════════════════════════════════

┌─ arithmetic
│  ✅ basic_int_ops
│  ✅ byte_int_mixing
│  ✅ byte_overflow
│  ✅ operator_precedence
│  ✅ type_promotion
└─ 5 passed, 0 failed

┌─ arrays
│  ✅ boundary_access
│  ✅ init_and_sum
│  ✅ iterate_and_modify
│  ...
└─ 7 passed, 0 failed

═══════════════════════════════════════
  Results: 32/32 passed
═══════════════════════════════════════
```
