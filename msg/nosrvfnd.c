/* msg\nosrvfnd.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 78,111,32,101,116,104,101,114,100,102,115,32,115,101,114,118
  S001 db 101,114,32,102,111,117,110,100,32,111,110,32,116,104,101,32
  S002 db 76,65,78,32,40,110,111,116,32,102,111,114,32,114,101,113
  S003 db 117,101,115,116,101,100,32,100,114,105,118,101,32,97,116,32
  S004 db 108,101,97,115,116,41,46,13,10,'$'
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