# Mongoose

Mongoose is an experimental compiled-language project written in ISO C.
The repository currently contains a working compiler frontend and the first
independently tested bytecode virtual-machine layer.

## Build

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Current command-line frontend

```sh
./build/bin/mongoose golden-master.snake
```

The executable currently runs these stages:

1. lexical analysis (tokens borrow slices of the source buffer);
2. parsing into a whole-program AST;
3. lexical-scope and semantic validation.

A successful run reports that the source is valid. It does **not yet** compile
that source into bytecode or execute it.

## Bytecode runtime foundation

The backend foundation is now present as a separate C library:

- `Value`: tagged runtime values (nil, boolean and number);
- `Chunk`: dynamically allocated bytecode and source-line metadata;
- constant pool: constants referenced by 16-bit indices;
- `VM`: stack-based interpreter for constants, arithmetic, comparisons,
  unary operations and return.

The `mongoose_vm_smoke` test constructs bytecode directly and verifies that
the VM evaluates `10 + 20` to `30`. The next integration step is compiling
the existing AST into this bytecode, then connecting execution to the CLI.

See [docs/LANGUAGE_GRAMMAR.md](docs/LANGUAGE_GRAMMAR.md),
[docs/FRONTEND_AUDIT.md](docs/FRONTEND_AUDIT.md), and
[docs/BACKEND_AUDIT.md](docs/BACKEND_AUDIT.md).
