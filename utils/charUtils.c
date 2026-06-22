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