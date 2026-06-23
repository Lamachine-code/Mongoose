#include "../utils/fileUtils.c"
#include "lexer.c"
#include "parser.c"
#include <stdio.h>

int main(int argc, char *argv[]) {
  (void)argc;
  const char *sourceCode = readSourceCode(argv);
  if (sourceCode == NULL) {
    return EXIT_FAILURE;
  }

  Lexer lexer;
  initLexer(&lexer, sourceCode);

  printf("--- Starting lexical analysis ---\n");
  Token *tokens = generateTokensList(&lexer);
  for (int i = 0; i < lexer.tokenCount; i++) {
    printf("[Line %d, Col %d] Type: %2d | Value: %.*s\n", tokens[i].line,
           tokens[i].col, tokens[i].type, tokens[i].length, tokens[i].start);
  }

  // 1. Initialize the parser
  Parser parser;
  initParser(&parser, tokens, lexer.tokenCount);

  printf("--- Parsing expression: %s\n ---", sourceCode);

  // 2. Start Pratt parsing at the lowest precedence (PREC_NONE = 0)
  // 3. Verify the tree
  printf("--- Generated syntax tree (parenthesized representation):\n ---\n");
  while (peekParser(&parser).type != TOKEN_EOF) {
    while (peekParser(&parser).type == TOKEN_NEWLINE) {
      advanceParser(&parser);
    }
    if (peekParser(&parser).type == TOKEN_EOF) {
      break;
    }
    ASTNode *root = parseStatement(&parser);
    printAST(root);
    printf("\n");

    // 4. Mandatory heap memory cleanup
    freeAST(root);
  }
  printf("AST memory freed successfully.\n");

  return 0;
}
