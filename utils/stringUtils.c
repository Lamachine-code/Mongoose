#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// Calculate the length of a string
// Already exists in <string.h> as "strlen"
int strLen(char *string) {
  int length = 0;
  int i = 0;

  while (string[i++] != '\0') {
    length++;
  }

  return length;
}

int tokenLen(const char *string) {
  int length = 0;
  int i = 0;

  while (string[i] != ' ' && string[i] != '\0') {
    i++;
    length++;
  }

  return length;
}

// Compare short strings
// Already exists in <string.h> as "strcmp"
bool strComp(const char *str1, const char *str2) {
  if (strlen(str1) != strlen(str2)) {
    return false;
  }
  size_t length = strlen(str1);
  return strncmp(str1, str2, length) == 0;
}

// void terminateString(char *str, size_t length) {
//   str[length] = '\0';
// }

// Takes a string as an argument and returns that same string terminated with “\0”
char* terminateString (const char* str, int strLength) {
  char* buf = (char*)malloc((strLength + 1) * sizeof(str));
  strncpy(buf, str, strLength);
  buf[strLength] = '\0';

  return buf;
}
