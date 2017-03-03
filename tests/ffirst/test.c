/*
 * Test program:
 * creates multiple *.del files, then perform FindFirst/FindNext iteration
 * over the directory and delete each entry. Finally check if no *.del files
 * are left.
 * This test is meant to verify that the implementation is properly handling
 * situations where the directory's content changes during a FindNext
 * iteration.
 *
 * Copyright (C) 2016 Mateusz Viste
 */

#include <dos.h>
#include <stdio.h>
#include <string.h>


unsigned short createfile(char *fn) {
  unsigned short axres = 0;
  _asm {
    mov ah, 3Ch
    mov cx, 20h
    mov dx, fn
    int 21h
    jc fail
    mov ah, 3Eh /* close file (handle in AX) */
    mov bx, ax
    int 21h
    jmp done
    fail:
    mov axres, ax
    done:
  }
  return(axres);
}


int lookfordelfiles(void) {
  int res = 0;
  char *pattern = "????????.DEL";
  _asm {
    mov ah, 4Eh   /* findfirst */
    mov cx, 0x7Fh /* all attribs */
    mov dx, pattern
    int 21h
    jnc found
    mov res, 1
    found:
  }
  return(res);
}


int main(int argc, char **argv) {
  unsigned short dta_seg = 0, dta_off = 0, myax = 0;
  unsigned short i = 0, count = 0;
  char *pattern = "????????.DEL";
  char *fn[] = {"file1.del", "HeLlO2.del", "yolo3.DEL", "4.del", "fiVe5.deL", "six6.del", "siedem7.del", "osiem8.del", NULL};

  if (argc != 1) {
    printf("Creates multiple *.del files, then perform FindFirst/FindNext iteration over\r\n"
           "the directory and delete each entry. Finally check if no *.del files are left.\r\n"
           "This test is meant to verify that the implementation is properly handling\r\n"
           "situations where the directory's content changes during a FindNext iteration.\r\n"
           "\r\n"
           "usage: test.com\r\n");
    return(1);
  }

  /* make sure no *.DEL files exist currently */
  if (lookfordelfiles() == 0) {
    printf("ERROR: some *.DEL files are already present\r\n");
    return(1);
  }

  /* create a few *.DEL files */
  for (i = 0; fn[i] != NULL; i++) {
    unsigned short res;
    res = createfile(fn[i]);
    if (res != 0) {
      printf("ERROR: failed to create file '%s' (AX=%u)\r\n", fn[i], res);
      return(res);
    }
  }

  /* get ptr to DTA */
  _asm {
    mov ah, 2fh
    int 21h
    mov dta_seg, es
    mov dta_off, bx
  }

  /* delete all *.DEL files, and count them */
  _asm {
    xor bx, bx   /* DX will be my counter */
    mov ah, 4Eh  /* findfirst */
    mov cx, 20h  /* file attribute (same as when used for creation) */
    mov dx, pattern
    int 21h
    jc nomore
    repeat:
    /* delete found file */
    push ds
    mov ax, dta_seg
    push ax
    pop ds
    mov ah, 41h
    mov dx, dta_off
    add dx, 1Eh
    int 21h
    pop ds
    /* findnext */
    inc bx
    mov ah, 4Fh
    int 21h
    jnc repeat
    nomore:
    mov myax, ax
    mov count, bx
  }
  if (count != i) {
    printf("ERROR: removed different number of files than created (expected: %u, removed: %u)\r\n", i, count);
    return(1);
  }
  if (myax != 0x12) {
    printf("ERROR: findfirst/findnext ended with an unexpected error (%02Xh instead of 12h)\r\n", myax);
    return(1);
  }

  /* finally make sure no *.DEL files are left */
  if (lookfordelfiles() == 0) {
    printf("ERROR: some *.DEL files are still present\r\n");
    return(1);
  }

  printf("ALL GOOD!\r\n");

  return(0);
}
