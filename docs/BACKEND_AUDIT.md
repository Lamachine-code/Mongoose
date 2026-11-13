# Mongoose backend — implementation audit

## Implemented

- Tagged runtime values: nil, booleans and numbers.
- Growable bytecode chunks with source-line metadata and a constant pool.
- 16-bit little-endian constant-pool indices.
- Stack VM with a 256-value operand stack.
- Arithmetic, floor division, modulo, power, unary operations and comparisons.
- 256 global variable slots, with define/get/set instructions.
- A native `print(value)` instruction exposed as a language builtin.
- AST-to-bytecode compilation for the currently executable subset.
- CLI pipeline: lex → parse → semantic analysis → compile → execute.
- Frontend-only checker retained as `mongoose_check` so syntax/semantic
  tests for not-yet-executable constructs remain useful.
- Regression fixture for global variables and arithmetic output.

## Executable source subset

- Top-level `let` declarations.
- Global variable reads and assignments.
- Numeric and boolean literals.
- `+`, `-`, `*`, `/`, `//`, `%`, `^`, `==`, `!=`, `<`, `>`.
- Unary minus and `not`.
- `print(value)` with exactly one argument.

The current compiler rejects unsupported constructs with a source-positioned
compile diagnostic rather than emitting partial bytecode and pretending the
program is executable.

## Runtime semantics and limitations

- Only nil and false are falsey; numeric zero is truthy.
- Arithmetic and ordering require numeric operands.
- Division, floor division and modulo by zero produce VM errors.
- Floor division uses mathematical floor.
- Modulo currently uses C `fmod` semantics; negative-operand behavior remains
  an open language-design decision.
- Equality compares same-typed primitive values; different types are unequal.
- Global slots are limited to 256 in this initial compiler/VM slice.
- Global slots are assigned in declaration order for each compiled program.
- Runtime error reporting currently identifies the error category but does not
  yet map the failing instruction back to its source line in the CLI.

## Not implemented

- Local variables and runtime lexical scopes.
- Conditional jumps, `if`, loops and short-circuit `and/or`.
- Function objects, call frames, parameters and returns.
- Arrays and `new_array()`.
- Strings, closures, modules and garbage collection.

## Next milestone

Add bytecode control flow and local scopes, then function call frames and
parameter/local-variable access. These are prerequisites for executing the
function and conditional constructs already accepted by the frontend.
