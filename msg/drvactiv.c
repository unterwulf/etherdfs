/* msg\drvactiv.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 84,104,101,32,114,101,113,117,101,115,116,101,100,32,100,114
  S001 db 105,118,101,32,108,101,116,116,101,114,32,105,115,32,97,108
  S002 db 114,101,97,100,121,32,105,110,32,117,115,101,46,32,80,108
  S003 db 101,97,115,101,32,99,104,111,111,115,101,32,97,110,111,116
  S004 db 104,101,114,13,10,100,114,105,118,101,32,108,101,116,116,101
  S005 db 114,46,13,10,'$'
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