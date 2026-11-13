# Mongoose

Mongoose is an experimental compiled-language project. The current branch
contains a C frontend prototype: lexer, parser, AST construction and a first
semantic-validation pass. Bytecode generation and VM execution are not part
of this frontend milestone.

## Build

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run the frontend

```sh
./build/bin/mongoose golden-master.snake
```

The executable reads one source file and runs the stages in order:

1. lexical analysis (tokens borrow slices of the source buffer);
2. parsing into a whole-program AST;
3. lexical-scope and semantic validation.

A successful run prints a short validation message. A failed run reports
the first diagnostic with its source line and column and exits non-zero.

## Current language surface

See [docs/LANGUAGE_GRAMMAR.md](docs/LANGUAGE_GRAMMAR.md) for the documented
syntax and semantic baseline, and [docs/FRONTEND_AUDIT.md](docs/FRONTEND_AUDIT.md)
for known limitations and remaining work.

This is not yet a bytecode compiler: the frontend validates source but does
not emit bytecode or execute programs.
