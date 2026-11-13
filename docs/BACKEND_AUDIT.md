# Mongoose backend — initial implementation audit

## Implemented

- A tagged `Value` representation for nil, booleans and numbers.
- A `Chunk` that owns a growable bytecode buffer, a parallel source-line
  array, and a growable constant pool.
- Constants are referenced by a 16-bit little-endian index following
  `OP_CONSTANT`.
- A stack-based VM with a fixed 256-value operand stack.
- Initial instructions: constants, nil/booleans, pop, arithmetic, floor
  division, modulo, power, numeric negation, truthiness negation, equality,
  numeric ordering and return.
- A CTest smoke test that manually emits bytecode for `10 + 20` and verifies
  the returned value is 30.
- Strict warning flags remain enabled for the VM library and its test target.

## Runtime semantics in this slice

- Only nil and false are falsey; all numbers (including zero) are truthy.
- Arithmetic and ordering require numeric operands.
- Division, floor division and modulo by zero return a VM error.
- Floor division uses mathematical floor, rather than truncation toward zero.
- Modulo currently uses C `fmod` semantics. This will need to be reconciled
  with the language's specified negative-operand behavior before it is treated
  as final language semantics.
- Equality compares values of the same primitive type; values of different
  types are unequal.

## Not implemented yet

- AST-to-bytecode compiler.
- Global and local variable storage.
- Conditional jumps and loops.
- Function objects, call frames and function invocation.
- Native functions such as `print()`.
- Arrays and `new_array()`.
- Runtime diagnostics with source locations exposed by the CLI.
- Garbage collection (not needed for the current primitive-only values).

## Next integration milestone

Compile numeric/boolean literals, unary and binary expressions, variable
declarations and variable reads into bytecode. Then add globals and a native
`print` function so a complete source file can produce visible output.

The current CLI remains frontend-only until that integration is complete.
