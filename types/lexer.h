#ifndef LEXER_H
#define LEXER_H

typedef enum {
  // Special tokens
  TOKEN_EOF,     // End of file (End of File)
  TOKEN_NEWLINE, // End of statement (\n)
  TOKEN_ERROR,   // Invalid token or lexical error

  // Identifiers and Literals
  TOKEN_IDENTIFIER, // ex: note, remplirPairs, new_array
  TOKEN_INT,        // ex: 15, 0, 2

  // Keywords
  TOKEN_LET,
  TOKEN_FUNCTION,
  TOKEN_IF,
  TOKEN_THEN,
  TOKEN_ELSE,
  TOKEN_STOP,
  TOKEN_TRUE,
  TOKEN_FALSE,

  // Logical operators (textual keywords)
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_NOT,

  // Operators and Punctuation
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_STAR,
  TOKEN_SLASH, // + - * /
  TOKEN_ASSIGN,
  TOKEN_EQUAL,
  TOKEN_NOTEQUAL, // = == !=
  TOKEN_LT,
  TOKEN_GT,
  TOKEN_LTEQ,
  TOKEN_GTEQ, // < > <= >=
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_COMMA, // ( ) ,
  TOKEN_LBRACKET,
  TOKEN_RBRACKET // [ ]
} TokenType;

typedef struct {
  TokenType type;
  const char *start; // Pointer to the start of the token in the source string
  int length;        // Length of the token string
  int line;          // For precise error reporting
  int col;           // Column where the token starts on the line
} Token;

typedef struct {
  const char *source;
  const char *start; // Pointer to the start of the token currently being
                     // processed/analyzed in the source string
  const char *current;
  int line;
  int col;        // Column of the current character
  int startCol;   // Column where the token starts
  int tokenCount; // Number of tokens
} Lexer;

void initLexer(Lexer *lexer, const char *source);
Token nextToken(Lexer *lexer);

#endif
