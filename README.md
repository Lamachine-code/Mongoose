# Mongoose

Mongoose is an experimental compiled-language project written in strict ISO C.
The command-line executable runs source through the frontend, bytecode compiler
and stack-based virtual machine.

## Build and test

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Run a program

```sh
./build/bin/mongoose tests/runtime_recursive_factorial.snake
```

The current executable subset supports:
- Global variables and assignments.
- Numeric and boolean values.
- Arithmetic, comparisons, unary minus and `not`.
- Conditional `if/else/stop` blocks.
- `while condition do ... stop` loops, including nested loops and loops inside functions.
- `break` exits the innermost loop; `continue` starts its next iteration.
- Builtin `print(value)`.
- User-defined functions, positional parameters, local variables, calls,
  return values and recursion.
- Short-circuit `and` and `or`.
- Dynamic arrays created with `new_array()`, indexed reads and writes, automatic growth with nil-filled gaps, and reference semantics when passed to functions.

Example:

```mongoose
function factorial(n)
    if n <= 1 then
        return 1
    else
        return n * factorial(n - 1)
    stop
stop

print(factorial(5))
```

Output:

```text
120
```

Strings, closures, modules and garbage collection are not executable yet. The frontend checker is retained separately
as `mongoose_check`, so the broader parser and semantic regression suite can
continue to validate syntax that the runtime compiler has not implemented.

## Architecture

1. Lexer: tokens borrow slices of the source buffer.
2. Parser: builds the whole-program AST.
3. Semantic analysis: validates lexical scopes and identifiers.
4. Bytecode compiler: lowers the executable AST subset into a `Chunk`.
5. VM: executes bytecode using tagged values, operand stack, global slots and
   per-call frames.

See [docs/LANGUAGE_GRAMMAR.md](docs/LANGUAGE_GRAMMAR.md),
[docs/FRONTEND_AUDIT.md](docs/FRONTEND_AUDIT.md), and
[docs/BACKEND_AUDIT.md](docs/BACKEND_AUDIT.md).
