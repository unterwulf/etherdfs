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

#include <stdio.h>


/* returns 0 on success, non-zero otherwise */
static int fileclose(unsigned short fhandle) {
  int res = 0;
  _asm {
    mov ah, 3eh /* close file */
    mov bx, fhandle
    mov res, 1
    int 21h
    jc done
    mov res, 0
    done:
  }
  return(res);
}

int main(int argc, char **argv) {
  unsigned short fhandle = 0, myax = 0, mycx = 0, mycflag = 0;
  char *fname;
  char *payload = "Hello!\r\n";
  if (argc != 2) {
    printf("creates a test file and tests DOS behaviour regarding INT21h/AX=6C00h calls\r\nusage: test file\r\n");
    return(1);
  }
  fname = argv[1];

  /* try opening non-existing file fname - should fail */
  printf("1. Try to open non-existing file (should fail)...\r\n");
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
  }
  printf("OK\r\n-----------------------\r\n");

  /* create & open fname, write a short string inside, then close it */
  printf("2. Create & open the file, but only if it doesn't exist...\r\n");
  _asm {
    mov ax, 6c00h
    mov bx, 1   /* open_mode = 1 (write access) */
    mov cx, 24h /* archive + system bits */
    xor dh, dh  /* dh is reserved */
    mov dl, 10h /* fail if file exists, create it if not */
    push si     /* save SI */
    mov si, fname
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov fhandle, ax
    mov myax, ax
    mov mycx, cx
    pop si      /* restore SI */
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  printf("3. Write some short payload to the file and close it...\r\n");
  _asm {
    mov ax, 4000h
    mov bx, fhandle
    mov cx, 8     /* number of bytes to write */
    mov dx, payload
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov myax, ax /* number of bytes actually written */
    mov mycx, cx
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (fileclose(fhandle) != 0) {
    printf("ERROR! File close failed!\r\n");
    return(1);
  }
  if ((mycflag != 0) || (myax != 8)) {
    printf("ERROR! Write failed or wrote incorrect amount of bytes!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  /* try creating file fname again (while it exists already) - should fail */
  printf("4. Try to create file that already exists (should fail)...\r\n");
  _asm {
    mov ax, 6c00h
    xor bl, bl  /* open_mode = 0 */
    mov cx, 20h /* archive bit only */
    xor dh, dh  /* dh is reserved */
    mov dl, 10h /* create if file does not exist, fail if it exists */
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
  }
  printf("OK\r\n-----------------------\r\n");

  /* open existing file fname */
  printf("5. Open existing file...\r\n");
  _asm {
    mov ax, 6c00h
    xor bl, bl  /* open_mode = 0*/
    mov cx, 20h /* archive bit only */
    xor dh, dh  /* dh is reserved */
    mov dl, 01h /* fail if file doesn't exists, open otherwise */
    push si     /* save SI */
    mov si, fname
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov fhandle, ax
    mov myax, ax
    mov mycx, cx
    pop si      /* restore SI */
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  /* Check file size */
  printf("6. Check file size...\r\n");
  _asm {
    mov ah, 42h  /* lseek */
    mov al, 2    /* al = 0 (seek from end) */
    mov bx, fhandle
    xor cx, cx /* CX:DX -> offset from origin */
    xor dx, dx
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov myax, ax
    mov mycx, cx
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if ((mycflag != 0) || (myax != 8)) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  if (fileclose(fhandle) != 0) {
    printf("ERROR! File close failed!\r\n");
    return(1);
  }

  printf("7. Check file attributes...\r\n");
  _asm {
    mov ax, 4300h
    mov dx, fname
    mov cx, 0
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov myax, ax
    mov mycx, cx
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if ((mycflag != 0) || (mycx != 0x24)) {
    printf("ERROR! Failed to fetch attributes or attributes not as expected!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  /* open existing file fname, truncating it, and close it */
  printf("8. Truncate file...\r\n");
  _asm {
    mov ax, 6c00h
    xor bl, bl  /* open_mode = 0 */
    mov cx, 24h /* archive + system bit (archive only fails on MSDOS 7) */
    xor dh, dh  /* dh is reserved */
    mov dl, 02h /* fail if file doesn't exists, truncate otherwise */
    push si     /* save SI */
    mov si, fname
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov fhandle, ax
    mov myax, ax
    mov mycx, cx
    pop si      /* restore SI */
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");
  if (fileclose(fhandle) != 0) {
    printf("ERROR! File close failed!\r\n");
    return(1);
  }

  /* open existing file fname */
  printf("9. Open existing file...\r\n");
  _asm {
    mov ax, 6c00h
    xor bl, bl  /* open_mode = 0*/
    mov cx, 20h /* archive bit only */
    xor dh, dh  /* dh is reserved */
    mov dl, 01h /* fail if file doesn't exists, open otherwise */
    push si     /* save SI */
    mov si, fname
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov fhandle, ax
    mov myax, ax
    mov mycx, cx
    pop si      /* restore SI */
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  /* check that its size is zero, then close it */
  printf("10. Check file size... (should be zero)\r\n");
  _asm {
    mov ah, 42h  /* lseek */
    mov al, 2    /* al = 0 (seek from end) */
    mov bx, fhandle
    xor cx, cx /* CX:DX -> offset from origin */
    xor dx, dx
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov myax, ax
    mov mycx, cx
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if ((mycflag != 0) || (myax != 0)) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  if (fileclose(fhandle) != 0) {
    printf("ERROR! File close failed!\r\n");
    return(1);
  }

  /* delete fname */
  printf("11. Delete file...\r\n");
  _asm {
    mov ah, 41h  /* delete file */
    xor cl, cl
    mov dx, fname
    mov mycflag, 1
    int 21h
    jc goterr
    mov mycflag, 0
    goterr:
    mov myax, ax
    mov mycx, cx
  }
  printf("AX=%04Xh CX=%04Xh CFLAG=%d\r\n", myax, mycx, mycflag);
  if (mycflag != 0) {
    printf("ERROR!\r\n");
    return(1);
  }
  printf("OK\r\n-----------------------\r\n");

  /* try to open non-existing file while truncating it (should fail) */
  printf("12. Try to truncate non-existing file (should fail)...\r\n");
  _asm {
    mov ax, 6c00h
    xor bl, bl  /* open_mode = 0*/
    mov cx, 20h /* archive bit only */
    xor dh, dh  /* dh is reserved */
    mov dl, 02h /* fail if file does not exist, truncate it if exists */
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
  }
  printf("OK\r\n-----------------------\r\n");

  printf("ALL GOOD!\r\n");

  return(0);
}
