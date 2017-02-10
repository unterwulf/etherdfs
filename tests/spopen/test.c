/*
 * test program, part of the EtherDFS project.
 * Copyright (C) 2017 Mateusz Viste
 *
 * open a file using the "extended open" call at INT 21/AX=6C00h
 *   AX = 6C00h
 *   BL = open mode
 *        bit 7   : inheritance
 *        bits 4-6: sharing mode
 *        bit 3   : reserved
 *        bits 0-2: access mode
 *   CX = attributes on creation
 *   DL = action if file exists/does not exist
 *        bits 7-4: action if file does not exist.
 *          0000 fail
 *          0001 create
 *        bits 3-0: action if file exists
 *          0000 fail
 *          0001 open
 *          0010 replace (truncate) & open
 *   DH = 00h (reserved)
 *   DS:SI = ASCIZ file name
 *
 * Return file handle in AX on success. CF set on error.
 *   CX = status (1=file opened, 2=file created, 3=file replaced)
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
  unsigned short fhandle = 0, myax = 0, mycx = 0, mycflag = 0;
  char *fname;
  if (argc != 2) {
    printf("creates a test file and tests DOS behaviour regarding INT21h/AX=6C00h calls\r\nusage: test file\r\n");
    return(1);
  }
  fname = argv[1];

  /* try opening non-existing file fname - should fail */
  printf("Try to open file (should fail)...\r\n");
  _asm {
    mov ax, 6c00h
    xor bl, bl  /* open_mode = 0*/
    mov cx, 20h /* archive bit only */
    xor dh, dh  /* dh is reserved */
    mov dl, 01h /* fail if file does not exist, open it if exists */
    push si     /* save SI */
    mov si, fname
    mov mycflag, 0
    int 21h
    jnc noerr
    mov mycflag, 1
    noerr:
    mov fhandle, ax
    mov myax, ax
    mov mycx, cx
    pop si      /* restore SI */
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (mycflag == 0) {
    printf("ERROR! The call should have failed!\r\n");
    fileclose(fhandle);
    return(1);
  } else {
    printf("OK\r\n-----------------------\r\n");
  }

  /* create & open fname, write a short string inside, then close it */
  /* open existing file fname, check that it's not empty and then close it */
  /* try creating file fname again (while it exists already) - should fail */
  /* open existing file fname, truncating it, and close it */
  /* open existing file fname and check that its size is zero, then close it */
  /* delete f1 */

  return(0);
}
