#include <fstream>
#include <iostream>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
      return 0;
    }
    char temp[100];
    for (int i = 0; i < 100; i++) {
      temp[i] = '\0';
    }
    strncpy(temp, "results/data_", 99);
    int index = 0;
    for (int i = 0; i < strlen(argv[1]); i++) {
      if (argv[1][i] == '/') {
        index = i;
      }
    }
    index++;
    strncat(temp, argv[1] + index, 99);
    FILE* fptr = fopen(temp, "a");
      if (fptr == NULL) {
      printf("Error reading \"%s\".\n", temp);
      return 0;
    }
    for (int i = 2; i < argc; i++) {
      fprintf(fptr, "%s", argv[i]);
      if (i != argc - 1) {
        fprintf(fptr, ",");
      }
    }
    fprintf(fptr, "\n");
    return 0;
}
