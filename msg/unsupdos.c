/* msg\unsupdos.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 85,110,115,117,112,112,111,114,116,101,100,32,68,79,83,32
  S001 db 118,101,114,115,105,111,110,33,32,69,116,104,101,114,68,70
  S002 db 83,32,114,101,113,117,105,114,101,115,32,77,83,45,68,79
  S003 db 83,32,53,43,46,13,10,'$'
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
