#include "../services/astService.c"
#include "../services/parserService.c"
#include <string.h>

ASTNode *parseExpression(Parser *parser, int minPrec);
ASTNode *parseStatement(Parser *parser);

static char *copyLexeme(Token token) {
  char *text = ensureAlloc(malloc(token.length + 1), "lexeme copy");
  memcpy(text, token.start, token.length);
  text[token.length] = '\0';
  return text;
}

static ASTNode **growNodeList(ASTNode **list, int count) {
  return ensureAlloc(realloc(list, count * sizeof(ASTNode *)), "node list");
}

static char **growStringList(char **list, int count) {
  return ensureAlloc(realloc(list, count * sizeof(char *)), "string list");
}

static int infixPrecedence(TokenType type) {
  switch (type) {
  case TOKEN_OR:
    return PREC_OR;
  case TOKEN_AND:
    return PREC_AND;
  case TOKEN_EQUAL:
  case TOKEN_NOTEQUAL:
    return PREC_EQUALITY;
  case TOKEN_LT:
  case TOKEN_GT:
  case TOKEN_LTEQ:
  case TOKEN_GTEQ:
    return PREC_COMPARISON;
  case TOKEN_PLUS:
  case TOKEN_MINUS:
    return PREC_TERM;
  case TOKEN_STAR:
  case TOKEN_SLASH:
    return PREC_FACTOR;
  default:
    return PREC_NONE;
  }
}

static ASTNode *parsePrimary(Parser *parser) {
  Token t = peekParser(parser);
  switch (t.type) {
  case TOKEN_INT:
    advanceParser(parser);
    return allocateLiteralNode(strtod(t.start, NULL));
  case TOKEN_TRUE:
    advanceParser(parser);
    return allocateBoolNode(true);
  case TOKEN_FALSE:
    advanceParser(parser);
    return allocateBoolNode(false);
  case TOKEN_IDENTIFIER:
    advanceParser(parser);
    return allocateIdentifierNode(copyLexeme(t));
  case TOKEN_LPAREN: {
    advanceParser(parser);
    ASTNode *inner = parseExpression(parser, PREC_NONE);
    consumeParser(parser, TOKEN_RPAREN,
                  "We were expecting ')' to close the group.");
    return inner;
  }
  case TOKEN_MINUS:
  case TOKEN_NOT: {
    advanceParser(parser);
    ASTNode *operand = parseExpression(parser, PREC_UNARY);
    return allocateUnaryOpNode(copyLexeme(t), operand);
  }
  default:
    fprintf(stderr, "Syntax error : unexpected token (Line %d, Col %d)\n",
            t.line, t.col);
    exit(EXIT_FAILURE);
  }
}

static ASTNode *parseCallArguments(Parser *parser, ASTNode *callee) {
  ASTNode **args = NULL;
  int argCount = 0;
  if (!checkParser(parser, TOKEN_RPAREN)) {
    for (;;) {
      ASTNode *arg = parseExpression(parser, PREC_NONE);
      args = growNodeList(args, argCount + 1);
      args[argCount++] = arg;
      if (!checkParser(parser, TOKEN_COMMA)) {
        break;
      }
      advanceParser(parser);
    }
  }
  consumeParser(parser, TOKEN_RPAREN,
                "We were expecting ')' after the arguments.");
  return allocateCallNode(callee, args, argCount);
}

static ASTNode *parsePostfix(Parser *parser, ASTNode *node) {
  for (;;) {
    if (checkParser(parser, TOKEN_LPAREN)) {
      advanceParser(parser);
      node = parseCallArguments(parser, node);
    } else if (checkParser(parser, TOKEN_LBRACKET)) {
      advanceParser(parser);
      ASTNode *index = parseExpression(parser, PREC_NONE);
      consumeParser(parser, TOKEN_RBRACKET,
                    "We were expecting ']' after the index.");
      node = allocateIndexNode(node, index);
    } else {
      return node;
    }
  }
}

ASTNode *parseExpression(Parser *parser, int minPrec) {
  ASTNode *left = parsePostfix(parser, parsePrimary(parser));

  for (;;) {
    Token op = peekParser(parser);
    int prec = infixPrecedence(op.type);
    if (prec <= minPrec) {
      break;
    }
    advanceParser(parser);
    ASTNode *right = parseExpression(parser, prec);
    left = allocateBinaryOpNode(copyLexeme(op), left, right);
  }

  return left;
}

static ASTNode *parseBlock(Parser *parser) {
  ASTNode **statements = NULL;
  int count = 0;

  for (;;) {
    while (checkParser(parser, TOKEN_NEWLINE)) {
      advanceParser(parser);
    }
    TokenType t = peekParser(parser).type;
    if (t == TOKEN_ELSE || t == TOKEN_STOP || t == TOKEN_EOF) {
      break;
    }
    ASTNode *stmt = parseStatement(parser);
    statements = growNodeList(statements, count + 1);
    statements[count++] = stmt;
  }

  return allocateBlockNode(statements, count);
}

static ASTNode *parseLetDeclaration(Parser *parser) {
  advanceParser(parser);
  Token name = consumeParser(parser, TOKEN_IDENTIFIER,
                             "We were expecting a variable name.");
  consumeParser(parser, TOKEN_ASSIGN,
                "We were expecting '=' after the variable name.");
  ASTNode *value = parseExpression(parser, PREC_NONE);
  return allocateVarDeclNode(copyLexeme(name), value);
}

static ASTNode *parseIfStatement(Parser *parser) {
  advanceParser(parser);
  ASTNode *condition = parseExpression(parser, PREC_NONE);
  consumeParser(parser, TOKEN_THEN,
                "We were expecting 'then' after the condition.");
  ASTNode *thenBranch = parseBlock(parser);

  ASTNode *elseBranch = NULL;
  if (checkParser(parser, TOKEN_ELSE)) {
    advanceParser(parser);
    elseBranch = parseBlock(parser);
  }

  consumeParser(parser, TOKEN_STOP,
                "We were expecting 'stop' to close the 'if'.");
  return allocateIfNode(condition, thenBranch, elseBranch);
}

static ASTNode *parseFunctionDeclaration(Parser *parser) {
  advanceParser(parser);
  Token name = consumeParser(parser, TOKEN_IDENTIFIER,
                             "We were expecting a function name.");
  consumeParser(parser, TOKEN_LPAREN,
                "We were expecting '(' after the function name.");

  char **params = NULL;
  int paramCount = 0;
  if (!checkParser(parser, TOKEN_RPAREN)) {
    for (;;) {
      Token p = consumeParser(parser, TOKEN_IDENTIFIER,
                              "We were expecting a parameter name.");
      params = growStringList(params, paramCount + 1);
      params[paramCount++] = copyLexeme(p);
      if (!checkParser(parser, TOKEN_COMMA)) {
        break;
      }
      advanceParser(parser);
    }
  }
  consumeParser(parser, TOKEN_RPAREN,
                "We were expecting ')' after the parameters.");

  ASTNode *body = parseBlock(parser);
  consumeParser(parser, TOKEN_STOP,
                "We were expecting 'stop' to close the function.");
  return allocateFunctionNode(copyLexeme(name), params, paramCount, body);
}

ASTNode *parseStatement(Parser *parser) {
  while (checkParser(parser, TOKEN_NEWLINE)) {
    advanceParser(parser);
  }

  if (checkParser(parser, TOKEN_LET)) {
    return parseLetDeclaration(parser);
  }
  if (checkParser(parser, TOKEN_IF)) {
    return parseIfStatement(parser);
  }
  if (checkParser(parser, TOKEN_FUNCTION)) {
    return parseFunctionDeclaration(parser);
  }

  ASTNode *expr = parseExpression(parser, PREC_NONE);
  if (checkParser(parser, TOKEN_ASSIGN)) {
    advanceParser(parser);
    ASTNode *value = parseExpression(parser, PREC_NONE);
    return allocateAssignNode(expr, value);
  }
  return expr;
}
