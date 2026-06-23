#include <stdio.h>
#include "lexer.c"
#include "parser.c"
#include "../utils/fileUtils.c"

int main(int argc, char *argv[]) {
    char* sourceCode = readSourceCode(argv);

    Lexer lexer;
    initLexer(&lexer, sourceCode);
    
    printf("--- Starting lexical analysis ---\n");
    Token* tokens = generateTokensList(&lexer);
    for (int i=0; i < lexer.tokenCount; i++) {
        printf("[Line %d, Col %d] Type: %2d | Value: %.*s\n", 
            tokens[i].line, tokens[i].col, tokens[i].type, tokens[i].length, tokens[i].start);
    }

    // 1. Initialize the parser
    Parser parser;
    initParser(&parser, tokens, lexer.tokenCount);

    printf("--- Parsing expression: %s\n ---", sourceCode);
    
    // 2. Start Pratt parsing at the lowest precedence (PREC_NONE = 0)
    ASTNode* root = parseExpression(&parser, PREC_NONE);

    // 3. Verify the tree
    printf("--- Generated syntax tree (parenthesized representation):\n ---");
    printAST(root);
    printf("\n");

    // 4. Mandatory heap memory cleanup
    freeAST(root);
    free(sourceCode);
    printf("AST memory freed successfully.\n");

    return 0;
}