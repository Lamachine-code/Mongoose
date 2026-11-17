# Mongoose backend — implementation audit

## Implemented

- Tagged runtime values: nil, booleans, numbers, strings, array references and dictionary references.
- Chunk-owned immutable string constants and VM-owned concatenation results.
- VM-owned dynamic arrays and dictionaries with indexed reads/writes and reference semantics, including nested collections and chained indexed reads/writes.
- Array literal construction, including empty, nested and computed-element literals, using `OP_NEW_ARRAY` and `OP_ARRAY_LITERAL_ADD`.\n- Dictionary literal construction, including empty dictionaries (`[:]`), nested values and duplicate-key replacement, using `OP_NEW_DICTIONARY` and `OP_DICTIONARY_LITERAL_ADD`.
- Bytecode chunks with source-line metadata, a constant pool and function metadata.
- Stack-based VM with arithmetic, comparisons, global variables and conditional jumps.
- AST-to-bytecode compilation and executable CLI pipeline.
- Builtin `print(value)`, including strings and `nil`.
- Source-level `nil` literal compiled using the existing `OP_NIL` instruction; nil equality uses existing `OP_EQUAL`.
- String concatenation with `+` and content-based equality.
- Generic `length(value)`: array element count, dictionary key count or UTF-8 code-point count for strings.
- Zero-based UTF-8 code-point indexing through `string[index]`, and code-point-range extraction through `substring(string, start, length)`.
- Array mutation builtins: `push(array, value)` appends and returns the new length; `pop(array)` removes and returns the last value, or nil for an empty array; `remove(array, index)` removes an indexed value and shifts later elements left.\n- Dictionary inspection and mutation builtins: `keys(dictionary)` and `values(dictionary)` return insertion-ordered arrays; `has_key(dictionary, key)` checks string-key membership; `delete(dictionary, key)` removes a string key and reports whether it existed.
- User-defined functions with positional parameters, local variables, return values and recursive calls.
- Per-call VM frames with a saved instruction pointer and operand-stack base.
- Local variable load/store instructions.
- Short-circuit evaluation for `and` and `or`.
- `break` and `continue` with nested-loop targeting and runtime scope cleanup.
- Inclusive `for` loops with one-time evaluated bounds and scoped iteration variables.
- Frontend-only checker retained as `mongoose_check`.

## Executable source subset

- Top-level `let` declarations, global reads and assignments, including `+=`, `-=`, `*=` and `/=` for variables and indexed collection values.
- Numeric, boolean, nil and string literals.
- Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `^`.
- Comparisons: `==`, `!=`, `<`, `>`, `<=`, `>=`.
- Unary minus and `not`.
- `print(value)` with exactly one argument.
- `if condition then ... else ... stop` and `while condition do ... stop` with block scopes.
- Function declarations, positional arguments, local `let`, calls and `return`.
- Recursive and forward function calls.
- Short-circuit `and` / `or`.
- `while condition do ... stop` loops with nested-loop support.
- `break` exits the innermost loop; `continue` jumps to its next condition check.
- Semantic analysis rejects `break` and `continue` outside loops.
- `new_array()` and array literals (`[]`, `[value, ...]`), indexed reads/writes, nested arrays (`matrix[row][column]`) and array arguments passed by reference. 
- Dictionary literals (`["key": value, ...]`, `[:]`), string-keyed indexed reads/writes, nested dictionaries/arrays and dictionary arguments passed by reference.

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
- Dictionary keys are strings; missing-key reads return `nil`. Closures, modules and garbage collection are not implemented.
- Runtime diagnostics do not yet map all failures to source lines.
- String indexing, length and substring operate on UTF-8 code points, not grapheme clusters. Invalid UTF-8 is rejected by these operations.
- Modulo still follows C `fmod` behavior for negative operands; language semantics remain to be specified.

## Regression coverage added in this milestone

- Basic function call and parameter passing.
- Local variable declarations and reads.
- Recursive factorial.
- Short-circuit `and` and `or`, using division by zero as a guard to prove the skipped operand is not evaluated.
- `break` and `continue`, nested-loop targeting, local cleanup on loop exit, and semantic rejection outside loops.
- Inclusive numeric `for` loops, one-time bound evaluation, loop-variable scope, and `continue` increment behavior.\n- Collection `for-in` loops over snapshot copies of array values or dictionary keys, including insertion order, loop-variable scope, and `continue` behavior.
- Nested-array reads, nested indexed assignment, and aliasing between an inner array and its containing array.
- Array literals: basic construction, nested literals, empty literals, and expression-valued elements. 
- Dictionaries: literal construction, empty dictionaries, reads, writes, missing keys, duplicate keys, nested collections, function aliasing, non-string key rejection, key/value extraction, membership and builtin argument validation.
- Compound assignments: all four arithmetic operators on variables, array elements and dictionary values; indexed target expressions are evaluated once.

Loop bytecode uses a backward `OP_LOOP` branch and forward conditional exit. Each compiler loop context records its condition target, lexical scope depth, variable-slot boundary and pending `break` jumps. A `break` emits a forward jump patched to the end of the innermost loop; a `continue` emits a backward jump to that loop's condition.

Before either jump, the compiler emits cleanup for variables introduced since the loop began. Scoped globals are reset with `OP_CLEAR_GLOBAL`; function locals are popped from the operand stack. The compiler's static scope bookkeeping is intentionally left unchanged while emitting this alternate control-flow path, so the ordinary fall-through path can still be generated correctly.


A numeric `for` loop creates a lexical scope containing its iteration variable and a hidden upper-bound slot. Both range expressions are evaluated once. The condition uses an inclusive comparison. `continue` jumps to the increment sequence; `break` exits and cleans up the entire loop scope.
\n\nA collection `for-in` loop evaluates its collection once and emits `OP_ITER_INIT`, which creates a VM-owned snapshot array. Arrays are copied as values; dictionaries are copied as their keys in insertion order. The compiler stores the snapshot, current index and snapshot length in hidden scoped slots, then uses ordinary indexed reads and loop branches. Mutating the original collection does not alter the active iteration.\n\n\n`OP_ARRAY_REMOVE` validates a non-negative integer index, returns the removed value and compacts the array in place. `OP_DICTIONARY_DELETE` validates the dictionary and string key, removes a matching entry by shifting subsequent entries, preserves the relative insertion order of the remaining keys, and returns a boolean indicating whether removal occurred.\n

Compound assignment lowering reuses the existing arithmetic opcodes. For a variable, the compiler loads its current value, evaluates the right-hand expression, applies the arithmetic opcode and stores the result. For an indexed target, it emits `OP_GET_INDEX_KEEP`: the VM reads the current element while retaining the collection and index on the operand stack. The arithmetic result can then be committed with the existing `OP_SET_INDEX`. This ensures side-effecting collection/index expressions execute exactly once.

Runtime diagnostics: each bytecode byte retains the source line supplied by the compiler. Before dispatching an instruction, the VM records that line in `VM.error_line`. If execution fails, the CLI combines the source filename, recorded line and the VM error description (for example, `program.snake:12: runtime error: division by zero`). For errors raised inside a called function, the line belongs to the instruction that actually failed.
