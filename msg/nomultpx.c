/* msg\nomultpx.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 70,97,105,108,101,100,32,116,111,32,102,105,110,100,32,97
  S001 db 110,32,97,118,97,105,108,97,98,108,101,32,73,78,84,32
  S002 db 50,70,32,109,117,108,116,105,112,108,101,120,32,105,100,46
  S003 db 13,10,89,111,117,32,109,97,121,32,104,97,118,101,32,108
  S004 db 111,97,100,101,100,32,116,111,111,32,109,97,110,121,32,84
  S005 db 83,82,115,32,97,108,114,101,97,100,121,46,13,10,'$'
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