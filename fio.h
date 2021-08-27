#include <stdio.h>
#include <stdlib.h>

__attribute__((unused))
static char* fio_read_text(const char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL)
    return NULL;
  fseek(f, 0, SEEK_END);
  size_t fsz = (size_t) ftell(f);
  fseek(f, 0, SEEK_SET);
  char *input = (char*)malloc((fsz+1)*sizeof(char));
  input[fsz] = 0;
  fread(input, sizeof(char), fsz, f);
  fclose(f);
  return input;
}

__attribute__((unused))
static void fio_write_text(const char *path, const char *text) {
  FILE *f = fopen(path, "w+");
  fputs(text, f);
  fclose(f);
}

