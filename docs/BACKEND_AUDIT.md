# Mongoose backend — implementation audit

## Implemented

- Tagged runtime values: nil, booleans, numbers and array references.
- VM-owned dynamic arrays with indexed reads/writes and reference semantics.
- Bytecode chunks with source-line metadata, a constant pool and function metadata.
- Stack-based VM with arithmetic, comparisons, global variables and conditional jumps.
- AST-to-bytecode compilation and executable CLI pipeline.
- Builtin `print(value)`.
- User-defined functions with positional parameters, local variables, return values and recursive calls.
- Per-call VM frames with a saved instruction pointer and operand-stack base.
- Local variable load/store instructions.
- Short-circuit evaluation for `and` and `or`.
- Frontend-only checker retained as `mongoose_check`.

## Executable source subset

- Top-level `let` declarations, global reads and assignments.
- Numeric and boolean literals.
- Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `^`.
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`.
- Unary minus and `not`.
- `print(value)` with exactly one argument.
- `if condition then ... else ... stop` with block scopes.
- Function declarations, positional arguments, local `let`, calls and `return`.
- Recursive and forward function calls.
- Short-circuit `and` / `or`.
- `new_array()`, indexed array reads, indexed writes and array arguments passed by reference.

## Runtime architecture

A call evaluates its arguments on the operand stack, then executes `OP_CALL`.
The VM records the caller's next instruction and the stack position at which
the callee's arguments begin. That position becomes local slot zero. Parameters
and local declarations are addressed relative to this base, so recursive calls
receive independent storage.

`OP_RETURN` removes the returned value, discards the callee's locals and
temporaries, restores the caller's stack position, pushes the return value,
and resumes at the saved instruction pointer.

Function entry points and arities are stored in the bytecode chunk. The
compiler predeclares function indices before compiling bodies, allowing
forward calls and recursion.

Short-circuit logical operations use conditional jumps that preserve the
left operand when it determines the result. Otherwise, the left operand is
discarded and the right operand is evaluated.

## Current limits

- Operand stack: 256 values.
- Call-frame depth: 64.
- Maximum function arity and local slot index are constrained by 8-bit operands.
- Maximum 255 user-defined functions in one chunk.
- Global slots: 256.
- Arrays, strings, loops, closures, modules and garbage collection are not implemented.
- Runtime diagnostics do not yet map all failures to source lines.
- Modulo still follows C `fmod` behavior for negative operands; language semantics remain to be specified.

## Regression coverage added in this milestone

- Basic function call and parameter passing.
- Local variable declarations and reads.
- Recursive factorial.
- Short-circuit `and` and `or`, using division by zero as a guard to prove the skipped operand is not evaluated.

The next backend milestone can focus on arrays and indexed assignment, which are
already represented in the frontend AST and are used by the golden-master
program.
