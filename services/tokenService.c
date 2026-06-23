#include "../types/lexer.h"
#include <stdio.h>
#include <stdlib.h>

const char *tokenTypeToString(TokenType type) {
  switch (type) {
  case TOKEN_LET:
    return "LET";
  case TOKEN_FUNCTION:
    return "FUNCTION";
  case TOKEN_IF:
    return "IF";
  case TOKEN_THEN:
    return "THEN";
  case TOKEN_ELSE:
    return "ELSE";
  case TOKEN_STOP:
    return "STOP";
  case TOKEN_TRUE:
    return "TRUE";
  case TOKEN_FALSE:
    return "FALSE";
  case TOKEN_AND:
    return "AND";
  case TOKEN_OR:
    return "OR";
  case TOKEN_NOT:
    return "NOT";
  case TOKEN_IDENTIFIER:
    return "IDENTIFIER";
  case TOKEN_INT:
    return "INT";
  case TOKEN_PLUS:
    return "PLUS";
  case TOKEN_MINUS:
    return "MINUS";
  case TOKEN_STAR:
    return "STAR";
  case TOKEN_SLASH:
    return "SLASH";
  case TOKEN_EQUAL:
    return "EQUAL_EQUAL";
  case TOKEN_NOTEQUAL:
    return "NOTEQUAL";
  case TOKEN_LT:
    return "LESS";
  case TOKEN_LTEQ:
    return "LESS_EQUAL";
  case TOKEN_GT:
    return "GREATER";
  case TOKEN_GTEQ:
    return "GREATER_EQUAL";
  case TOKEN_ASSIGN:
    return "ASSIGN";
  case TOKEN_LPAREN:
    return "LEFT_PAREN";
  case TOKEN_RPAREN:
    return "RIGHT_PAREN";
  case TOKEN_LBRACKET:
    return "LEFT_BRACKET";
  case TOKEN_RBRACKET:
    return "RIGHT_BRACKET";
  case TOKEN_COMMA:
    return "COMMA";
  case TOKEN_NEWLINE:
    return "NEWLINE";
  case TOKEN_EOF:
    return "EOF";
  case TOKEN_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

// If "tokens" is NULL, realloc() behaves exactly like malloc().
Token *reallocTokens(Token *tokens, int nb_tokens) {
  Token *temp = realloc(tokens, nb_tokens * sizeof(Token));

  if (!temp) {
    fprintf(stderr, "Error: Memory reallocation for the token list failed\n");
    ;
    exit(EXIT_FAILURE);
  }

  return temp;
}

Token *allocTokens(int nb_tokens) {
  Token *temp = (Token *)malloc(nb_tokens * sizeof(Token));

  if (!temp) {
    fprintf(stderr, "Error: Memory reallocation for the token list failed");
    exit(EXIT_FAILURE);
  }

  return temp;
}
