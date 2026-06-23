#include "../services/parserService.c"
#include "../services/astService.c"
#include <stdio.h>

ASTNode* parseExpression(Parser* parser, int precedence) {

    // Token lhs_token = consumeParser(parser, TOKEN_INT, "We were expecting a number.");
    Token lhs_token = advanceParser(parser);
    ASTNode* lhs = NULL;

    if (lhs_token.type == TOKEN_INT) {
        lhs = allocateLiteralNode( strtod(lhs_token.start, NULL) );

    } else if (lhs_token.type == TOKEN_MINUS) {
        ASTNode* operand = parseExpression(parser, PREC_UNARY);
        lhs = allocateUnaryOpNode(lhs_token.start, operand);

    } else {
        fprintf(stderr, "Syntax Error (Line %d, Col %d)", lhs_token.line, lhs_token.col);
        exit(EXIT_FAILURE);
    }

    while (true)
    {
        Token op = peekParser(parser);
        int op_precedence = getPrecedence(op.type);
        if ( op.type == TOKEN_EOF || op_precedence <= precedence ) {
            break;
        }

        advanceParser(parser);
        ASTNode* rhs = parseExpression(parser, op_precedence);
        lhs = allocateBinaryOpNode(op.start, lhs, rhs);
    }

    return lhs;
}
