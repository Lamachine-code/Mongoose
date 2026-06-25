#ifndef FILEUTILS_C
#define FILEUTILS_C

#include <stdio.h>
#include <stdlib.h>

char* readTxtFile(const char* fileName) {
    FILE *fptr = fopen(fileName, "r");
    if (fptr == NULL) {
        printf("Error: Could not open file.\n");
        return NULL;
    }

    // Seek to the end to determine the size
    fseek(fptr, 0, SEEK_END);
    long size = ftell(fptr);
    rewind(fptr);

    // Allocate a buffer
    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("Error: Memory allocation failed.\n");
        fclose(fptr);
        // return NULL;
        exit(EXIT_FAILURE);
    }

    // Read the whole file
    fread(buffer, 1, size, fptr);
    buffer[size] = '\0'; // Terminate the string

    fclose(fptr);
    return buffer; // Caller must free the returned buffer
}


char* readSourceCode(char *argv[]) {
    if (!argv[1]) {
        printf("Error: You didn't specify the name of the file.");
        exit(EXIT_FAILURE);
    }
    return readTxtFile(argv[1]);
}

// Ensure that the file pointer is not NULL
FILE* safeFOpen (char* filename, char* mode) {
    FILE* fptr;

    fptr = fopen(filename, mode); // returns the address of the file loaded in the memory
    if (!fptr) {
        printf("Error: Could not open file.\n");
        exit(EXIT_FAILURE);
    }

    return fptr;
}

#endif

