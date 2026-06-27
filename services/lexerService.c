#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../types/lexer.h"
#include <ctype.h>

void printSyntaxErrMsg(int line, int col, char* msg);

void initLexer(Lexer* lexer, const char* source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->col = 1;
    lexer->startCol = 1;
    lexer->tokenCount = 0;
}

// Compute the length of the lexeme currently being scanned
int computeLength (Lexer* lexer) {
    return (int)(lexer->current - lexer->start);
}

// Check if the char is valid for an identifier or a keyword
// Valid characters: '_' and Alphanumeric characters
bool isValidChar(char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

// Check if we've reached the end of the source file
static bool isAtEnd(Lexer* lexer) {
    return *(lexer->current) == '\0';
}

// Return the current character and advance the pointer (consume the character: read and increment).
static char advance(Lexer* lexer) {
    lexer->current++;
    lexer->col++;
    return lexer->current[-1];
}

// Examine (return) the current character without consuming it
static char peek(Lexer* lexer) {
    return *(lexer->current);
}

// Examine (return) the current+1 character without consuming it
static char peekNext(Lexer* lexer) {
    char* nextChar = (char*) lexer->current + 1;
    return *nextChar;
}

// Consume the current character only if it matches the expected one
// Consuming a character: read it and then increment "lexer.current".
static bool match(Lexer* lexer, char expected) {
    if (isAtEnd(lexer)) return false;
    if (*(lexer->current) != expected) return false;
    lexer->current++;
    return true;
}

// Create a token on the fly (zero-copy)
static Token makeToken(Lexer* lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = computeLength(lexer);
    token.line = lexer->line;
    token.col = lexer->startCol;

    return token;
}

// Advance through the source while there are spaces, tabs, newlines or comments (# ...).
void skipWhitespaceAndComments(Lexer* lexer) {
    for (;;) {
        char c = peek(lexer); // Look at the character without advancing
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance(lexer); // consume the space
                break;
            case '#':
                // Found a comment! Advance until the end of the line
                // NOTE: do NOT consume the '\n' or the '\0'
                while (peek(lexer) != '\n' && peek(lexer) != '\0') {
                    advance(lexer); // consume
                }
                break;
            default:
                return; // Useful character, '\0' or '\n', stop and return control to nextToken
        };
    }
}

static void consumeNumbers(Lexer* lexer) {
    while (isdigit(peek(lexer))) {
        advance(lexer);
    }
}

Token number(Lexer* lexer) {
    consumeNumbers(lexer);

    /* Implement logic to handle error cases */

    // Check for decimal part
    if (peek(lexer) == '.') {
        if (!isdigit(peekNext(lexer))) {
            printSyntaxErrMsg(lexer->line, lexer->col + 1, "We were expecting integer at that point.\n");
            exit(EXIT_FAILURE);
        }
        advance(lexer); // Consume the "." because the "consumeNumbers" function will not consume it. 
        consumeNumbers(lexer);
    }

    Token token = makeToken(lexer, TOKEN_NUMBER);
    return token;
}

static TokenType checkKeyword(Lexer* lexer, int startOffset, int length, const char* keyword, TokenType type) {
    // Compute the actual length of the word being analyzed
    int currentLength = computeLength(lexer);
    
    // If the total length matches and the substring is identical
    if (currentLength == startOffset + length && 
        strncmp(lexer->start + startOffset, keyword, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static Token identifierOrKeyword(Lexer* lexer) {
    // Consume all alphanumeric characters or underscores
    while (isValidChar(peek(lexer))){
        advance(lexer);
    }
    
    // Determine the token type (state machine based on the first letter)
    char firstChar = lexer->start[0];
    TokenType type = TOKEN_IDENTIFIER;
    
    switch (firstChar) {
        case 'a': type = checkKeyword(lexer, 0, 3, "and", TOKEN_AND); break;
        case 'l': type = checkKeyword(lexer, 0, 3, "let", TOKEN_LET); break;
        case 'f':
            // 'f' can be "function" or "false"
            // Hint: check the second character to branch or chain checkKeyword calls
            if (computeLength(lexer) > 1) { // Ensure the lexeme has at least two characters
                if (lexer->start[1] == 'u') type = checkKeyword(lexer, 0, 8, "function", TOKEN_FUNCTION);
                else if (lexer->start[1] == 'a') type = checkKeyword(lexer, 0, 5, "false", TOKEN_FALSE);
            }
            break;
        case 'i': type = checkKeyword(lexer, 0, 2, "if", TOKEN_IF); break;
        case 't':
            if (computeLength(lexer) > 1) {
                if (lexer->start[1] == 'h') type = checkKeyword(lexer, 0, 4, "then", TOKEN_THEN);
                else if (lexer->start[1] == 'r') type = checkKeyword(lexer, 0, 4, "true", TOKEN_TRUE); 
            }
            break;
        case 'e': type = checkKeyword(lexer, 0, 4, "else", TOKEN_ELSE); break;
        case 's': type = checkKeyword(lexer, 0, 4, "stop", TOKEN_STOP); break;
        case 'o': type = checkKeyword(lexer, 0, 2, "or", TOKEN_OR); break;
        case 'n': type = checkKeyword(lexer, 0, 3, "not", TOKEN_NOT); break;
    }    
    return makeToken(lexer, type);
}

Token nextToken(Lexer* lexer) {
    lexer->tokenCount++;
    // 1. Clean up spaces and comments
    skipWhitespaceAndComments(lexer);

    lexer->start = lexer->current;
    lexer->startCol = lexer->col;

    // 2. Check for end of file
    if (isAtEnd(lexer)) {
        return makeToken(lexer, TOKEN_EOF);
    }

    char c = advance(lexer); // After this line, "lexer->current" advances to the next character. "c" contains the consumed character.
    // Example: in "let c =3", if "c = advance(lexer)" returns "=", then after execution "lexer->current" will point at "3".

    // 3. Token recognition
    if (isalpha(c)) return identifierOrKeyword(lexer);
    if (isdigit(c)) return number(lexer);

    switch (c) {
        case '\n':
            // This version had an issue with line numbering
            // Fix the line numbering issue
            Token token = makeToken(lexer, TOKEN_NEWLINE);
            lexer->line++;
            lexer->col = 1;
            return token;
        case '(': return makeToken(lexer, TOKEN_LPAREN);
        case ')': return makeToken(lexer, TOKEN_RPAREN);
        case '[': return makeToken(lexer, TOKEN_LBRACKET);
        case ']': return makeToken(lexer, TOKEN_RBRACKET);
        case ',': return makeToken(lexer, TOKEN_COMMA);
        case '+': return makeToken(lexer, TOKEN_PLUS);
        case '-': return makeToken(lexer, TOKEN_MINUS);
        case '*': return makeToken(lexer, TOKEN_STAR);
        case '/': return makeToken(lexer, TOKEN_SLASH);
        
        // Operators that may be two-character tokens
        case '=': 
            return match(lexer, '=') ? makeToken(lexer, TOKEN_EQUAL) : makeToken(lexer, TOKEN_ASSIGN);
        case '!': 
            if (match(lexer, '=')) return makeToken(lexer, TOKEN_NOTEQUAL);
            break;
        case '<': 
            return match(lexer, '=') ? makeToken(lexer, TOKEN_LTEQ) : makeToken(lexer, TOKEN_LT);
        case '>': 
            return match(lexer, '=') ? makeToken(lexer, TOKEN_GTEQ) : makeToken(lexer, TOKEN_GT);
    }

    return makeToken(lexer, TOKEN_ERROR);
}

