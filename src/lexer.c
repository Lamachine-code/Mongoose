#include "../services/lexerService.c"
#include "../services/tokenService.c"
#include <string.h>

// Return the list of all tokens
Token* generateTokensList(Lexer* lexer) {
    // Initialise a list of 20 tokens
    int nb_token = 20;
    Token* tokens = allocTokens(nb_token);

    int i = 0;
    while (true)
    {
        // If the list is complete
        if (lexer->tokenCount >= nb_token) {
            // Resize the array to hold 20 more tokens
            nb_token += 20;
            tokens = reallocTokens(tokens, nb_token);
        }

        tokens[i] = nextToken(lexer);

        // If we encounter the last token in the source code
        if (tokens[i].type == TOKEN_EOF) {
            // Shrink the list to "lexer.tokenCount" elmts
            tokens = reallocTokens(tokens, lexer->tokenCount);
            break;
        }
        i++;
    }
    
    return tokens;
}
