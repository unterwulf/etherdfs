/* msg\instlled.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 101,116,104,101,114,100,102,115,32,105,110,115,116,97,108,108
  S001 db 101,100,32,40,108,111,99,97,108,32,77,65,67,32,'$'
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
