# Closures: runtime representation and implementation plan

## Status

Architecture decision for the closure foundation. This document does not change language behavior or bytecode execution.

## Current execution model

- `Chunk` owns bytecode, constants, and a table of `FunctionInfo` records.
- A `FunctionInfo` currently describes a statically registered function with an entry offset, arity, and source-backed name.
- `OP_CALL` carries a function-table index. The VM derives the callee from that index and creates a `CallFrame` containing the return instruction pointer, stack base, function index, and call-site line.
- Locals live in the VM value stack and are addressed relative to the current frame.
- The parser represents a call using a function name, not an arbitrary callee expression. The compiler resolves that name statically.
- The semantic analyzer already walks nested lexical scopes, but function calls and compiler code generation are still based on the current static function model.

These constraints mean closures require coordinated changes to the AST/call model, semantic resolution, compiler, VM, and collector. They are not just an additional opcode.

## Chosen runtime model

Keep immutable code metadata separate from mutable runtime environments:

1. **Function prototype** — immutable metadata owned by the `Chunk`. It identifies the function's bytecode entry, arity, name, and upvalue descriptors. It is not a garbage-collected runtime object.
2. **Closure** — a VM-owned heap object pairing a prototype index with an array of pointers to captured upvalues. A callable value refers to a closure, not directly to a code offset.
3. **Upvalue** — a VM-owned heap object representing one captured variable. While its defining activation is alive, it is *open* and points to the variable's stack slot. When that activation exits, it becomes *closed* and stores the value itself.
4. **Shared capture** — the VM maintains a canonical open-upvalue entry for each captured stack slot. If multiple closures capture the same local, they point to the same Upvalue object, preserving mutation and aliasing semantics.
5. **Indirect calls** — a later bytecode operation will consume a closure value and arguments, rather than requiring the compiler to encode a statically known function-table index.

The initial implementation should use explicit `FunctionInfo` metadata in `Chunk`, and separate runtime `Closure` and `Upvalue` structs. Avoid turning every local into an individually allocated object: only captured locals need upvalues.

## Ownership and lifetime

- `Chunk` owns function prototypes and their upvalue-descriptor arrays. Prototype names continue to follow the existing source-name lifetime contract unless a later source ownership change is made.
- `VM` owns runtime closures and upvalues, just as it currently owns arrays, strings, and dictionaries.
- A closure references its prototype by stable function-table index, avoiding a dangling pointer if the function table is reallocated during compilation.
- A closure owns its array of upvalue pointers, but does not own the pointed-to upvalue objects.
- An open upvalue points into the VM stack and is linked into the VM's open-upvalue list. It must be closed before the corresponding stack slots are discarded or reused.
- Closing an upvalue copies the current stack value into the upvalue's closed storage and redirects its value accessor to that storage.
- VM teardown frees each registered closure and upvalue exactly once. The closure's pointer array is freed with the closure; upvalues are freed by the VM registry.

## Garbage collection requirements

The tracing collector must eventually understand these edges:

- Roots: stack values, globals, VM result, bytecode constants, and the VM's open-upvalue list.
- Closure -> captured Upvalue objects.
- Upvalue -> captured Value (whether open or closed).
- An open upvalue's stack slot remains a root through the stack; its list membership also keeps the Upvalue object itself alive.
- A closed upvalue traces its stored Value.
- Function prototypes are chunk-owned metadata, not GC objects; a closure's prototype index therefore does not add a heap edge.

New runtime objects must be rooted before any subsequent allocation can trigger collection. Allocation and registration must be failure-safe: a partially built closure must not leave dangling registry entries or leak its pointer array.

## Implementation sequence

1. Add and test the data structures, constructors/accessors, and VM ownership registries without changing compiler output or VM call behavior.
2. Extend tracing and teardown for closure/upvalue objects, including open and closed upvalue tests.
3. Add function upvalue descriptors and nested-function compilation.
4. Add lexical upvalue resolution and bytecode operations to capture, read, write, and close upvalues.
5. Add first-class function values and indirect calls; update parser and semantic analysis to accept callable expressions.
6. Add end-to-end tests for returned closures, shared mutable captures, nested captures, recursion, and collection of unreachable closure cycles.

Each stage must preserve the existing test suite and be committed only to `development/ai`.

## Non-goals for the foundation stage

- No new source syntax.
- No nested-function bytecode generation.
- No change to `OP_CALL` or `OP_RETURN`.
- No change to existing function declaration, lookup, or call semantics.
- No migration of all local variables to heap allocations.
