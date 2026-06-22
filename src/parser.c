#include "../services/parserService.c"
#include "../services/astService.c"

ASTNode* parseExpression(Parser* parser, int precedence) {

    Token lhs_token = consumeParser(parser, TOKEN_INT, "We were expecting a number.");
    ASTNode* lhs = allocateLiteralNode( strtod(lhs_token.start, NULL) );

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
