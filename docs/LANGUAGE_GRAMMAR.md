# Mongoose language grammar (frontend baseline)

This grammar documents the source syntax currently supported by the Mongoose frontend, compiler and VM.

## Lexical conventions

- Identifiers contain ASCII letters, digits and underscores; the first
  character is a letter or underscore.
- Numeric literals are decimal integers or decimal fractions.
- `true` and `false` are boolean literals; `nil` is the absence-of-value literal.
- Double-quoted strings support `\\n`, `\\r`, `\\t`, `\\0`, `\\\\` and `\\"` escapes.
- String indexing and substring offsets are zero-based UTF-8 Unicode code-point indices.
- Newlines separate statements.
- Keywords are `let`, `function`, `if`, `then`, `else`, `stop`, `while`, `do`,
  `for`, `to`, `in`, `nil`, `return`, `and`, `or` and `not`.
- `#` starts a line comment.
- `//` starts a line comment when it is the first non-whitespace text on a
  line; otherwise it is the floor-division operator. This preserves the
  comments in `golden-master.snake` and still permits expressions such as
  `quotient = numerator // denominator`.

## EBNF

```ebnf
program        = separators, { declaration | statement, separators }, EOF ;
declaration    = variableDecl | functionDecl ;
variableDecl   = "let", IDENTIFIER, "=", expression ;
functionDecl   = "function", IDENTIFIER, "(", [ parameters ], ")",
                 separators, block, "stop" ;
parameters     = IDENTIFIER, { ",", IDENTIFIER } ;

statement      = ifStmt | whileStmt | forStmt | breakStmt | continueStmt | returnStmt | assignmentOrExpression ;
whileStmt      = "while", expression, "do", separators, block, "stop" ;
forStmt        = "for", IDENTIFIER, ( "=", expression, "to", expression | "in", expression ), "do", separators, block, "stop" ;
ifStmt         = "if", expression, "then", separators, block,
                 [ "else", separators, block ], "stop" ;
returnStmt     = "return", expression ;
breakStmt      = "break" ;
continueStmt   = "continue" ;
block          = { declaration | statement, separators } ;
assignmentOrExpression = expression ;

expression     = assignment ;
assignment     = logical, [ "=", assignment ] ;
logical        = equality, { ("and" | "or"), equality } ;
equality       = comparison, { ("==" | "!="), comparison } ;
comparison     = term, { ("<" | "<=" | ">" | ">="), term } ;
term           = factor, { ("+" | "-"), factor } ;
factor         = unary, { ("*" | "/" | "%" | "//"), unary } ;
unary          = ("-" | "not"), unary | power ;
power          = postfix, [ "^", unary ] ;
postfix        = primary, { "(", [ arguments ], ")" | "[", expression, "]" } ;
arguments      = expression, { ",", expression } ;
primary        = NUMBER | STRING | "true" | "false" | "nil" | IDENTIFIER
               | arrayLiteral | dictionaryLiteral | "(", expression, ")" ;
arrayLiteral   = "[", [ expression, { ",", expression } ], "]" ;
dictionaryLiteral = "[", ( ":", "]" | STRING, ":", expression,
                    { ",", STRING, ":", expression } ), "]" ;
separators     = { NEWLINE } ;
```

## Semantic baseline

- A variable is visible only after its declaration in the same execution
  sequence; nested blocks may shadow names from outer scopes.
- Two declarations with the same name in one scope are an error.
- Function parameters are local to the function body.
- Functions may call themselves and other functions declared in the same
  lexical scope.
- Reading an undeclared identifier is an error.
- Assignment targets must be identifiers or indexed expressions.
- Calls must resolve to a function (except recognized builtins) and use the
  expected number of arguments.
- `return` is valid only inside a function.
- `break` and `continue` are valid only inside `while` or `for` loops; they target the innermost loop.
- Numeric `for` uses an inclusive upper bound; start and end expressions are evaluated once, and its variable is scoped to the loop.\n- Collection `for ... in ...` iterates over an array snapshot of values or a dictionary snapshot of keys, preserving array order and dictionary insertion order. The collection expression is evaluated once; changes to the original collection during iteration do not change the iteration snapshot. The loop variable is scoped to the loop.
- `nil` is distinct from `false`, `0` and the empty string; `==` and `!=` compare it by value type.
- `length(value)` returns an array's element count, a dictionary's distinct-key count or a string's UTF-8 code-point count. Other value types cause a runtime type error.\n- `keys(dictionary)` and `values(dictionary)` return arrays in dictionary insertion order. `has_key(dictionary, key)` returns a boolean and requires a string key. These builtins require a dictionary argument.
- `string[index]` returns one complete UTF-8 code point as a string. `substring(string, start, length)` extracts a range of code points; invalid indices and ranges are runtime errors. Invalid UTF-8 encountered by these operations is a runtime error.
- `push(array, value)` appends one value and returns the resulting array length. `pop(array)` removes and returns the last value; popping an empty array returns `nil`. `remove(array, index)` removes the indexed element, shifts subsequent elements left and returns the removed value; an invalid index is a runtime error. These builtins require an array.\n- `delete(dictionary, key)` removes a string key and returns `true` if it existed, otherwise `false`. It requires a dictionary and a string key. Remaining entries retain their relative insertion order.
- Array literals use square brackets: `[10, 20, 30]`; the empty literal `[]` is valid. Elements are expressions and may themselves be arrays.
- Arrays may contain arrays. Chained indexing such as `matrix[row][column]` supports nested reads and assignments; nested arrays retain reference semantics. 
- Dictionaries use string-literal keys, for example `["name": "Alice"]`; `[:]` creates an empty dictionary. Indexed reads of absent keys return `nil`, and indexed assignment inserts or replaces a key. Duplicate keys in a literal keep the last value. Dictionaries retain reference semantics and may be nested in arrays or other dictionaries. `length(dictionary)` returns its number of distinct keys. Updating a key preserves its original insertion position.
- This baseline has no static type system; it does not infer or enforce
  numeric/boolean/array types.

## Open design boundary

Array and dictionary literals are supported by the parser, compiler and VM. Classes, imports and user-defined types are not currently part of the language.
