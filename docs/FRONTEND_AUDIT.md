# Mongoose compiler frontend — audit

This document records the state of the repository at the start of the
`feature/compiler-frontend-completion` work. The checked-in code is the source
of truth; this audit does not assume that planned features are already
implemented.

## Existing components

- A zero-copy lexer: tokens retain a pointer and a length into the source
  buffer.
- A Pratt expression parser and statement parser.
- A tagged-union AST for literals, identifiers, unary/binary expressions,
  declarations, blocks, conditionals, functions, calls, indexing, returns and
  assignments.
- A lexical environment implementation with nested parent scopes, local
  duplicate detection and parent-chain lookup.

## Issues found

1. The command-line entry point parses only one statement, not a complete
   source file.
2. Parser errors call `exit()`. This prevents the frontend from being reused
   by tests, another program or an editor, and prevents reporting multiple
   diagnostics.
3. The parser's token cursor can index before the token array when given an
   empty stream; its EOF behavior is not represented as a safe invariant.
4. Several dynamic allocations in function parameters, call arguments and
   AST vectors are not checked before use.
5. The AST stores function parameter names as pointers without their lengths,
   unlike the rest of the zero-copy identifier representation.
6. The lexer does not update the column when it consumes the second character
   of a compound token.
7. The lexer accepts identifiers beginning with a letter only, although its
   identifier scanner otherwise accepts underscores.
8. The sample program uses `//` comments, while the lexer currently treats
   `//` as floor division and recognizes `#` comments instead.
9. The source is built by including implementation `.c` files from other
   implementation files. This hides dependencies and makes independent
   compilation and testing difficult.
10. There is no committed automated test suite or CI workflow.

## Scope of the frontend completion

The initial language surface is based on the existing token/AST model and
`golden-master.snake`: numeric and boolean literals; identifiers; arithmetic,
comparison and logical operators; variable declarations; assignments;
function declarations and calls; returns; conditionals; blocks; and array
indexing. The builtin `new_array()` is recognized as a language-level builtin
for semantic validation, not implemented here.

The frontend deliverable is:

- deterministic tokenization with useful source diagnostics;
- parsing of a complete source file into one program AST;
- AST ownership and cleanup that are safe on successful and failed parses;
- lexical scope and semantic validation;
- a documented grammar and repeatable tests.

Bytecode generation, VM execution, runtime array behavior and a GUI are
outside this frontend task.
