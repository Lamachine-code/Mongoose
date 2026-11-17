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
- Global variables and assignments, including compound arithmetic assignments (`+=`, `-=`, `*=`, `/=`) on variables and indexed array/dictionary values.
- Numeric, boolean, nil and string values, including escaped string literals.
- Arithmetic, comparisons, unary minus and `not`.
- The `nil` literal for absence of a value; compare with `==` or `!=`.
- Conditional `if/else/stop` blocks.
- `while condition do ... stop` loops, including nested loops and loops inside functions.
- `break` exits the innermost loop; `continue` starts its next iteration.
- Inclusive numeric `for variable = start to end do ... stop` loops, plus `for variable in collection do ... stop` loops over array values or dictionary keys.
- Builtin `print(value)`.
- User-defined functions, positional parameters, local variables, calls,
  return values and recursion.
- Short-circuit `and` and `or`.
- String concatenation with `+`, content-based string equality and printing.
- `length(value)` returns the number of elements in an array, distinct keys in a dictionary or UTF-8 code points in a string; `string[index]` returns one complete UTF-8 code point; `substring(string, start, length)` extracts a range of code points. Indices are zero-based.
- Arrays support `push(array, value)`, `pop(array)` and `remove(array, index)`. `remove` deletes the indexed element, shifts later elements left and returns the removed value.\n- Dictionaries support `keys(dictionary)` and `values(dictionary)` in insertion order, `has_key(dictionary, key)` for membership, and `delete(dictionary, key)` to remove a string key. `delete` returns `true` when a key existed and was removed, otherwise `false`.
- Array literals such as `[10, 20, 30]`, including empty literals `[]`, computed elements and nested literals. Arrays retain indexed reads/writes, automatic growth with nil-filled gaps, and reference semantics when passed to functions. The `new_array()` constructor remains available. Multidimensional access and mutation work with chained indexing such as `matrix[row][column]`.

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

Closures and modules are not implemented yet. The VM includes a tracing mark-and-sweep garbage collector for arrays, strings and dictionaries. The frontend checker is retained separately
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


## Dictionaries

Mongoose supports dictionaries with string keys. Use `["key": value]` to
create one and `[:]` for an empty dictionary:

```mongoose
let person = ["name": "Alice", "age": 25]
print(person["name"])
person["age"] = 26
print(length(person))
```

Reading a missing key returns `nil`. Assigning to an existing key replaces
its value; assigning to a missing key inserts it. Updating an existing key preserves its insertion position. `keys(person)` and `values(person)` return arrays in insertion order; `has_key(person, "name")` checks membership. `for key in person do ... stop` iterates dictionary keys in insertion order. `delete(person, "age")` removes a key without changing the relative order of remaining entries. Dictionaries are reference
values and can be nested in arrays, dictionaries and function arguments.

## Diagnostics

Frontend failures report the source file, one-based line and column, and the
phase that rejected the program (`lexical error`, `syntax error`, `semantic
error`, or `compile error`). Both `mongoose` and `mongoose_check` use the same
format so editor integrations and command-line users can distinguish the
origin of a failure.
