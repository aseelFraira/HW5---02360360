# FuncC Compiler

A compiler for **FuncC**, a statically-typed, C-like language. The compiler implements a full pipeline — lexical analysis, parsing, semantic analysis, and **LLVM IR** code generation — producing executable programs via the LLVM toolchain.

## Features

- **Lexical Analysis** — Flex-based scanner that tokenizes keywords, identifiers, literals, and operators
- **Parsing** — Bison-generated LALR(1) parser that builds an Abstract Syntax Tree (AST)
- **AST & Visitor Pattern** — Rich node hierarchy with a visitor interface for clean, extensible tree traversal
- **Semantic Analysis** — Type checking, scope management, and validation via `SemanticVisitor` and a scoped `SymbolTable`
- **LLVM IR Code Generation** — `codeGvisitor` traverses the AST and emits LLVM IR, including runtime-linked print functions

## Language Overview

| Category | Details |
|---|---|
| **Types** | `int`, `byte`, `bool`, `void`, `string` literals |
| **Arrays** | Declaration & indexing with runtime bounds checking |
| **Expressions** | Arithmetic (`+`,`-`,`*`,`/`), relational (`<`,`>`,`<=`,`>=`,`==`,`!=`), logical (`and`,`or`,`not`), type casts |
| **Control Flow** | `if`/`else`, `while`, `break`, `continue`, `return` |
| **Functions** | Declarations with typed parameters, forward references, recursion |

## Project Structure

```
├── src/
│   ├── main.cpp               # Entry point — ties all stages together
│   ├── ast/
│   │   ├── nodes.hpp / .cpp   # AST node definitions
│   │   └── visitor.hpp        # Visitor interface (base class)
│   ├── lexer/
│   │   └── scanner.lex        # Flex lexer specification
│   ├── parser/
│   │   └── parser.y           # Bison grammar & AST construction
│   ├── semantic/
│   │   ├── SemanticVisitor.*   # Semantic analysis pass
│   │   └── SymbolTable.*      # Scoped symbol table
│   └── codegen/
│       ├── codeGvisitor.*     # LLVM IR code generation pass
│       ├── output.*           # Error reporting & CodeBuffer for IR emission
│       └── print_functions.llvm
├── tests/
│   ├── run_tests.sh           # Automated test runner
│   ├── README.md              # Testing guide
│   └── allTests/              # Test suites (*.in / *.out pairs)
├── examples/                  # LLVM IR example programs
├── .gitignore
├── Makefile
├── CMakeLists.txt
└── README.md
```

## Build

### Prerequisites

- **g++** with C++17 support
- **Flex** (lexer generator)
- **Bison** (parser generator)
- **LLVM** (`lli`) — to run the generated IR

### Compile

```bash
make
```

### Clean

```bash
make clean
```

## Usage

```bash
./hw5 < program.fc
```

The compiler reads FuncC source from **stdin** and writes LLVM IR to **stdout**. Pipe the output to `lli` to execute:

```bash
./hw5 < program.fc | lli
```

If a lexical, syntax, or semantic error is encountered, an appropriate error message is printed and compilation stops.

## Example

```c
void main() {
    int x = 5;
    int y = 10;
    printi(x + y);
}
```

```bash
$ ./hw5 < example.fc | lli
15
```
