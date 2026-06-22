#include <string.h>
#include <stdbool.h>

// Calculate the length of a string
// Already exists in <string.h> as "strlen"
int strLen(char* string) {
    int length = 0;
    int i =0;

    while (string[i++] != '\0') {
        length++;
    }

    return length;
}

// Compare short strings
// Already exists in <string.h> as "strcmp"
bool strComp(const char* str1, const char* str2) {
    if (strlen(str1) != strlen(str2)) {
        return false;
    }
    int length;
    return strncmp(str1, str2, length);
}