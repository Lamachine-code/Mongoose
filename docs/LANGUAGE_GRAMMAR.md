# Mongoose language grammar (frontend baseline)

This grammar documents the frontend implemented on the
`feature/compiler-frontend-completion` branch. It describes the supported
source syntax; bytecode generation and execution are separate backend work.

## Lexical conventions

- Identifiers contain ASCII letters, digits and underscores; the first
  character is a letter or underscore.
- Numeric literals are decimal integers or decimal fractions.
- `true` and `false` are boolean literals.
- Newlines separate statements.
- Keywords are `let`, `function`, `if`, `then`, `else`, `stop`,
  `return`, `and`, `or` and `not`.
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

statement      = ifStmt | returnStmt | assignmentOrExpression ;
ifStmt         = "if", expression, "then", separators, block,
                 [ "else", separators, block ], "stop" ;
returnStmt     = "return", expression ;
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
primary        = NUMBER | "true" | "false" | IDENTIFIER
               | "(", expression, ")" ;
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
- This baseline has no static type system; it does not infer or enforce
  numeric/boolean/array types.

## Open design boundary

The language currently has no string literals, array literals, loops,
classes, imports or user-defined types in its AST. These should not be added
implicitly as part of the frontend stabilization work.
