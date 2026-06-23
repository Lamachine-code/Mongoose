#include "../types/parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Initialize the parser with the array of tokens provided by the Lexer
void initParser(Parser *parser, Token *tokens, int tokenCount) {
  parser->tokens = tokens;
  parser->tokenCount = tokenCount;
  parser->current = 0; // Always start at the first token (index 0)
}

// Peek at the current token without advancing the cursor
Token peekParser(Parser *parser) {
  // Safety: if we go past the array, return the last token (usually EOF)
  if (parser->current >= parser->tokenCount) {
    return parser->tokens[parser->tokenCount - 1];
  }
  return parser->tokens[parser->current];
}

// Check if the current token is of an expected type, without consuming it
bool checkParser(Parser *parser, TokenType type) {
  if (parser->current >= parser->tokenCount)
    return false;
  return peekParser(parser).type == type;
}

// =================================================================
// TODO: Complete the two functions below
// =================================================================

// Advance the cursor by one token and return the token that was passed.
// Be careful not to overflow the array (can't advance past tokenCount - 1).
// Parser golden rule for advance: "If I'm not at the end, increment, but in ALL
// cases return the token that was previously pointed to."
Token advanceParser(Parser *parser) {
  // 1. Save the currently pointed token
  Token t = peekParser(parser);

  // 2. Increment ONLY if there are still tokens to read
  if (parser->current < parser->tokenCount) {
    parser->current++;
  }

  // 3. Return the saved token
  return t;
}

// Expect a specific token type. If it's correct, consume it (advance).
// If it's not the expected type, print the error message and exit(1).
Token consumeParser(Parser *parser, TokenType type, const char *message) {
  // 1. Use checkParser to verify the current token matches the expected 'type'
  bool isValid = checkParser(parser, type);

  // 2. If yes: call advanceParser() and return its result
  if (isValid) {
    Token t = advanceParser(parser);
    return t;
  }
  // 3. If not: print the message with fprintf(stderr, ...) and call exit(1)
  // Properly write to the standard error stream (stderr)
  Token currentToken = peekParser(parser);
  fprintf(stderr, "Syntax error : %s (Line %d, Col %d) \n", message,
          currentToken.line, currentToken.col);
  exit(EXIT_FAILURE);

  // TODO: Implement the logic here
}
