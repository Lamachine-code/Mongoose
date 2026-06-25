#ifndef CHARUTILS_C
#define CHARUTILS_C

#include <stdbool.h>

bool isAlpha(char c) {
    return c >= 'a' && c <= 'z' || 
           c >= 'A' && c <= 'Z';
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isAphaNum(char c) {
    return isAlpha(c) || isDigit(c);
}

// Converts a single character into a string (character array). The function simply adds “\0” to the memory location immediately following the one occupied by that character.
char* charToString(char c) {
    char* buf = malloc(2);
    buf[0] = c;
    buf[1] = '\0';
    return buf;
}

#endif