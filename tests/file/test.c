/*
 * Opens a file, writes content to it, then opens it again and write new content
 * in the middle. Should print to screen "Hello, World! This is Mateusz."
 */

#include <stdio.h>
#include <stdlib.h>


int main(void) {
  FILE *fd;
  fd = fopen("test.txt", "wb");
  if (fd == NULL) {
    printf("Error: failed to create file\r\n");
    return(1);
  }
  /* write stuff */
  fprintf(fd, "Hello, xxxxx! This is Mateusz.\r\n");
  fclose(fd);

  /* open file again, and write "World" to offset 7 */
  fd = fopen("test.txt", "r+b");
  if (fd == NULL) {
    printf("Error: failed to open file\r\n");
    return(1);
  }
  fseek(fd, 7, SEEK_SET);
  fprintf(fd, "World");
  fclose(fd);

  fd = fopen("test.txt", "rb");
  if (fd == NULL) {
    printf("Error: failed to open file\r\n");
    return(1);
  }
  for (;;) {
    int c;
    c = fgetc(fd);
    if (c == EOF) break;
    printf("%c", c);
  }
  fclose(fd);

  return(0);
}
