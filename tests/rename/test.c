/*
 * test program, part of the EtherDFS project.
 * Copyright (C) 2017 Mateusz Viste
 *
 * creates a file and tries to rename it:
 *  - once as a new name,
 *  - once as an already existing file name
 *  - once as an already existing directory name
 *  - once as wildcard
 *
 * check for proper error catching at each step.
 *   AH = 56h
 *   DS:DX = ASCIZ filename of existing file
 *   ES:DI = ASCIZ new filename (no wildcard)
 *
 * Return CF clear if successful, otherwise error code in AX
 */

#include <stdio.h>


/* returns 1 if file exists, 0 otherwise */
int fexist(char *fname) {
  int res = 0;
  _asm {
    mov ax, 3D04h  /* AH=3Dh open existing file / AL=04h as read-only */
    mov dx, fname
    int 21h
    jc failed
    mov res, 1
    /* file exists (handle in AX) - close it now */
    mov bx, ax  /* file handle */
    mov ah, 3Eh /* close file */
    int 21h
    failed:
  }
  return(res);
}


int main(int argc, char **argv) {
  unsigned short myax = 0, mycflag = 0;
  char *fname1 = "first.txt";
  char *fname2 = "second.txt";
  char *fname3 = "wil?card.txt";

  if (argc != 1) {
    printf("tests various 'rename file' actions\r\nusage: test\r\n");
    return(1);
  }

  /* make sure none of the files exist */
  if ((fexist(fname1) != 0) || (fexist(fname2) != 0)) {
    printf("One of the to-be-tested files already exists. Make sure none of these exists:\r\n  %s\r\n  %s\r\n", fname1, fname2);
    return(1);
  }

  printf("1. create file '%s'... ", fname1);
  _asm {
    mov ah, 3Ch    /* create new file */
    mov cx, 20h    /* file attribs */
    mov dx, fname1 /* asciz filename */
    int 21h
    mov myax, ax
    jnc created
    mov mycflag, 1
    jmp abort
    created:
    /* file created, now close its handle */
    mov bx, ax   /* file handle */
    mov ah, 3Eh  /* close file */
    int 21h
    mov myax, ax
    jnc abort
    mov mycflag, 1
    abort:
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }

  /* rename file1 as file2 */
  printf("2. rename '%s' to '%s'... ", fname1, fname2);
  _asm {
    push es
    push di
    mov ah, 56h  /* rename file */
    mov dx, fname1 /* DS:DX - existing file name */
    push ds
    pop es
    mov di, fname2 /* ES:DI - new file name */
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
    pop di
    pop es
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }

  printf("3. verify that '%s' exists... ", fname2);
  if (fexist(fname2) == 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n");

  printf("4. verify that '%s' doesn't exist anymore... ", fname1);
  if (fexist(fname1) != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n");

  printf("5. create '%s' again... ", fname1);
  _asm {
    mov ah, 3Ch    /* create new file */
    mov cx, 20h    /* file attribs */
    mov dx, fname1 /* asciz filename */
    int 21h
    mov myax, ax
    jnc created
    mov mycflag, 1
    jmp abort
    created:
    /* file created, now close its handle */
    mov bx, ax   /* file handle */
    mov ah, 3Eh  /* close file */
    int 21h
    mov myax, ax
    jnc abort
    mov mycflag, 1
    abort:
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }

  printf("6. try renaming '%s' to '%s' (should fail)... ", fname2, fname1);
  _asm {
    push es
    push di
    mov ah, 56h  /* rename file */
    mov dx, fname2 /* DS:DX - existing file name */
    push ds
    pop es
    mov di, fname1 /* ES:DI - new file name */
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
    pop di
    pop es
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag == 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  mycflag = 0;

  /* delete fname2 */
  printf("7. Delete file '%s'... ", fname2);
  _asm {
    mov ah, 41h  /* delete file */
    xor cl, cl
    mov dx, fname2
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }

  printf("8. try renaming '%s' to '%s' (should fail)... ", fname1, fname3);
  _asm {
    push es
    push di
    mov ah, 56h  /* rename file */
    mov dx, fname1 /* DS:DX - existing file name */
    push ds
    pop es
    mov di, fname3 /* ES:DI - new file name */
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
    pop di
    pop es
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag == 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  mycflag = 0;

  printf("9. create directory '%s'... ", fname2);
  _asm {
    mov ah, 39h  /* mkdir */
    mov dx, fname2
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  mycflag = 0;

  printf("10. try renaming '%s' to '%s' (should fail)... ", fname1, fname2);
  _asm {
    push es
    push di
    mov ah, 56h  /* rename file */
    mov dx, fname1 /* DS:DX - existing file name */
    push ds
    pop es
    mov di, fname2 /* ES:DI - new file name */
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
    pop di
    pop es
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag == 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  mycflag = 0;

  /* delete fname1 */
  printf("11. Delete file '%s'... ", fname1);
  _asm {
    mov ah, 41h  /* delete file */
    xor cl, cl
    mov dx, fname1
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }

  /* rmdir fname2 */
  printf("12. rmdir '%s'... ", fname2);
  _asm {
    mov ah, 3Ah  /* rmdir */
    mov dx, fname2
    int 21h
    mov myax, ax
    jnc allgood
    mov mycflag, 1
    allgood:
  }
  printf("AX=%04Xh CFLAG=%d\r\n", myax, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }

  printf("*** ALL GOOD! ***\r\n");

  return(0);
}
