# Mongoose

Mongoose is an experimental compiled-language project written in strict ISO C.
The current command-line executable now runs a first end-to-end source program
through the frontend, bytecode compiler and stack-based virtual machine.

## Build and test

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run a program

```sh
./build/bin/mongoose tests/runtime_globals.snake
```

The first executable runtime slice supports top-level global variable
declarations, variable reads and assignments, numeric/boolean literals,
arithmetic and basic equality/order comparisons, unary minus and `not`,
conditional `if/else/stop` blocks, and the one-argument builtin `print(value)`.

Example:

```mongoose
let a = 10
let b = 20
let result = a + b
print(result)
```

Output:

```text
30
```

This is an intentionally small executable subset. Functions, conditional
statements, nested runtime scopes, arrays, short-circuit logical operators,
and user-defined function calls are not executable yet. The frontend checker
is retained separately as `mongoose_check`, so the broader parser and semantic
regression suite can continue to validate syntax that the runtime compiler
has not implemented.

## Architecture

1. Lexer: tokens borrow slices of the source buffer.
2. Parser: builds the whole-program AST.
3. Semantic analysis: validates lexical scopes and identifiers.
4. Bytecode compiler: lowers the supported AST subset into a `Chunk`.
5. VM: executes bytecode using a tagged `Value` stack and global slots.

See [docs/LANGUAGE_GRAMMAR.md](docs/LANGUAGE_GRAMMAR.md),
[docs/FRONTEND_AUDIT.md](docs/FRONTEND_AUDIT.md), and
[docs/BACKEND_AUDIT.md](docs/BACKEND_AUDIT.md).
