# HW5 — Compiler for a Custom Language (Course 02360360)

A full compiler pipeline for a C-like language, built as Homework 5 for the Compilation course. The compiler reads source code, performs lexical analysis, parsing, semantic checking, and emits **LLVM IR** as output.

## Features

- **Lexical Analysis** — Flex-based scanner that tokenizes keywords, identifiers, literals, and operators
- **Parsing** — Bison-generated LALR(1) parser that builds an Abstract Syntax Tree (AST)
- **AST & Visitor Pattern** — Rich node hierarchy with a visitor interface for clean, extensible tree traversal
- **Semantic Analysis** — Type checking, scope management, and validation via `SemanticVisitor` and a `SymbolTable`
- **LLVM IR Code Generation** — `codeGvisitor` emits LLVM IR text, including runtime print functions

## Supported Language Constructs

| Category | Details |
|---|---|
| **Types** | `int`, `byte`, `bool`, `void`, `string` literals |
| **Arrays** | Declaration & indexing with bounds checking |
| **Expressions** | Arithmetic (`+`,`-`,`*`,`/`), relational (`<`,`>`,`<=`,`>=`,`==`,`!=`), logical (`and`,`or`,`not`), type casts |
| **Control Flow** | `if`/`else`, `while`, `break`, `continue`, `return` |
| **Functions** | Declarations with typed parameters, forward references |

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
│   ├── hw5-tests/             # Test inputs & expected outputs
│   ├── simple_tests/          # Test runner & extended suites
│   └── selfcheck-hw5          # Self-check script
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

### Compile

```bash
make
```

This runs Flex on `scanner.lex`, Bison on `parser.y`, then compiles everything into the `hw5` executable.

### Clean

```bash
make clean
```

## Usage

```bash
./hw5 < input_file.txt
```

The compiler reads from **stdin** and writes LLVM IR to **stdout**. If a lexical, syntax, or semantic error is encountered, an appropriate error message is printed and compilation stops.
