/* msg\mapfail.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 85,110,97,98,108,101,32,116,111,32,97,99,116,105,118,97
  S001 db 116,101,32,116,104,101,32,108,111,99,97,108,32,100,114,105
  S002 db 118,101,32,109,97,112,112,105,110,103,46,32,89,111,117,32
  S003 db 97,114,101,32,101,105,116,104,101,114,32,117,115,105,110,103
  S004 db 32,97,110,13,10,117,110,115,117,112,112,111,114,116,101,100
  S005 db 32,111,112,101,114,97,116,105,110,103,32,115,121,115,116,101
  S006 db 109,44,32,111,114,32,121,111,117,114,32,76,65,83,84,68
  S007 db 82,73,86,69,32,100,105,114,101,99,116,105,118,101,32,100
  S008 db 111,32,110,111,116,32,112,101,114,109,105,116,13,10,116,111
  S009 db 32,100,101,102,105,110,101,32,116,104,101,32,114,101,113,117
  S00A db 101,115,116,101,100,32,100,114,105,118,101,32,108,101,116,116
  S00B db 101,114,46,13,10,'$'
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
