/*
 * Gets timestamp from file, and try to set it to something else.
 */

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void fileclose(unsigned short fhandle) {
  _asm {
    mov ah, 3eh /* close file */
    mov bx, fhandle
    int 21h
  }
}

int main(int argc, char **argv) {
  unsigned short fhandle = 0, ftime = 0, fdate = 0;
  static char fname[256];
  if (argc != 2) {
    printf("reads the timestamp of a file, and tries to set it to 1985-07-29, 00:00\r\nusage: test file\r\n");
    return(1);
  }
  strcpy(fname, argv[1]);

  /* open file */
  _asm {
    mov fhandle, 0ffffh
    mov al, 2 /* read */
    mov ah, 3dh /* open existing file */
    push dx
    mov dx, offset fname
    int 21h
    /* jump if failed */
    jc failed
    mov fhandle, ax
    failed:
    pop dx
  }
  if (fhandle == 0xffffu) {
    printf("ERROR: failed to open file '%s'\r\n", fname);
    return(1);
  }

  /* get file time/date */
  _asm {
    /* zero out ftime and fdate */
    mov ftime, 0ffffh
    mov fdate, 0ffffh
    /* */
    mov ax, 5700h
    mov bx, fhandle /* file handle */
    int 21h
    /* if failed (CF set), jump to end */
    jc failed
    mov ftime, cx
    mov fdate, dx
    failed:
  }
  if ((ftime == 0xffffu) || (fdate == 0xffffu)) {
    fileclose(fhandle);
    printf("ERROR: failed to get file timestamp\r\n");
    return(1);
  }

  /* fdate: YYYYYYYMMMMDDDDD
     ftime: HHHHHMMMMMMSSSSS */
  printf("current timestamp: %04u-%02u-%02u %02u:%02u:%02u\r\n", (fdate >> 9) + 1980, (fdate >> 5) & 15, fdate & 31, ftime >> 11, (ftime >> 5) & 63, (ftime & 31) << 1);

  /* set time/date */
  ftime = 0;
  fdate = 0xafd;
  _asm {
    mov ax, 5701h
    mov bx, fhandle
    mov cx, ftime /* new time */
    mov dx, fdate /* new date */
    int 21h
    jnc good
    mov fdate, 0ffffh
    good:
  }
  fileclose(fhandle);

  if (fdate == 0xffffu) {
    printf("ERROR: failed to set file's timestamp\r\n");
    return(1);
  } else {
    printf("Setting new file timestamp: SUCCESS\r\n");
  }

  return(0);
}
