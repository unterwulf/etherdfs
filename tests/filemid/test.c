/*
 * Opens a file, writes content to it, then opens it again and write new content
 * in the middle. Should print to screen "Hello, World! This is Mateusz."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char **argv) {
  char buff[64];
  int i;
  char *fname;
  FILE *fd;

  if (argc != 2) {
    printf("Creates a new file and writes stuff to it. Then opens it, and writes minor new\r\nstuff in the middle. Then opens it again and checks whether the file's content\r\nseems correct. Should print to screen \"Hello, World! This is Mateusz.\"\r\n\r\nusage: test file\r\n");
    return(1);
  }
  fname = argv[1];

  /* try to open file -> should fail */
  fd = fopen(fname, "rb");
  if (fd != NULL) {
    fclose(fd);
    printf("ERROR: file '%s' already exists!\r\n", fname);
    return(1);
  }

  fd = fopen(fname, "wb");
  if (fd == NULL) {
    printf("Error: failed to create file\r\n");
    return(1);
  }
  /* write stuff */
  fprintf(fd, "Hello, xxxxx! This is Mateusz.\r\n");
  fclose(fd);

  /* open file again, and write "World" to offset 7 */
  fd = fopen(fname, "r+b");
  if (fd == NULL) {
    printf("Error: failed to open file\r\n");
    return(1);
  }
  fseek(fd, 7, SEEK_SET);
  fprintf(fd, "World");
  fclose(fd);

  fd = fopen(fname, "rb");
  if (fd == NULL) {
    printf("Error: failed to open file\r\n");
    return(1);
  }
  for (i = 0; i < 63; i++) {
    int c;
    c = fgetc(fd);
    if (c == EOF) break;
    buff[i] = c;
    printf("%c", c);
  }
  buff[i] = 0;
  fclose(fd);

  if (strcmp(buff, "Hello, World! This is Mateusz.\r\n") != 0) {
    printf("ERROR: unexpected file content\r\n");
    return(1);
  }

  printf("Test passed successfully.\r\n");
  unlink(fname);
  return(0);
}
