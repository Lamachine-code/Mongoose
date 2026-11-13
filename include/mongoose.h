#ifndef MONGOOSE_H
#define MONGOOSE_H
#include <stdbool.h>
#include <stddef.h>

typedef enum {
 T_EOF, T_ERROR, T_NEWLINE, T_IDENT, T_NUMBER,
 T_LET, T_FUNCTION, T_IF, T_THEN, T_ELSE, T_STOP, T_TRUE, T_FALSE, T_RETURN,
 T_AND, T_OR, T_NOT,
 T_PLUS, T_MINUS, T_STAR, T_SLASH, T_FLOOR_DIV, T_PERCENT, T_POWER,
 T_ASSIGN, T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,
 T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET, T_COMMA
} TokenType;
typedef struct { TokenType type; const char *start; size_t length; int line, column; } Token;
typedef struct { Token *items; size_t count; } TokenArray;
typedef struct { char message[256]; int line, column; } Diagnostic;
bool lex_source(const char *source, TokenArray *out, Diagnostic *error);
void free_tokens(TokenArray *tokens);

typedef enum {
 N_NUMBER, N_BOOL, N_IDENT, N_UNARY, N_BINARY, N_VAR, N_BLOCK,
 N_IF, N_FUNCTION, N_CALL, N_INDEX, N_RETURN, N_ASSIGN, N_PROGRAM
} NodeKind;
typedef struct Node Node;
typedef struct { const char *name; size_t length; } Name;
struct Node {
 NodeKind kind;
 Token token;
 union {
  double number; bool boolean; Name name;
  struct { Token op; Node *operand; } unary;
  struct { Token op; Node *left, *right; } binary;
  struct { Name name; Node *value; } var;
  struct { Node **items; size_t count, capacity; } list;
  struct { Node *condition, *then_branch, *else_branch; } conditional;
  struct { Name name; Name *params; size_t param_count; Node *body; } function;
  struct { Name name; Node **args; size_t arg_count, arg_capacity; } call;
  struct { Node *target, *index; } index;
  struct { Node *value; } ret;
  struct { Node *target, *value; } assign;
 } as;
};
typedef struct { Node *root; } Ast;
bool parse_tokens(const TokenArray *tokens, Ast *out, Diagnostic *error);
void free_ast(Ast *ast);
bool analyze_ast(const Ast *ast, Diagnostic *error);
#endif
