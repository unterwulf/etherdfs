/* msg\unloaded.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 69,116,104,101,114,68,70,83,32,117,110,108,111,97,100,101
  S001 db 100,32,115,117,99,99,101,115,115,102,117,108,108,121,46,13
  S002 db 10,'$'
 getip:
  pop dx
  push cs
  pop ds
  mov ah,9h
  int 21h
  pop ax
  pop dx
  pop ds
};
