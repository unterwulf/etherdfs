/* msg\help.c: THIS FILE IS AUTO-GENERATED BY GENMSG.C -- DO NOT MODIFY! */
_asm {
  push ds
  push dx
  push ax
  call getip
  S000 db 69,116,104,101,114,68,70,83,32,118,48,46,54,32,47,32
  S001 db 67,111,112,121,114,105,103,104,116,32,40,67,41,32,50,48
  S002 db 49,55,32,77,97,116,101,117,115,122,32,86,105,115,116,101
  S003 db 13,10,65,32,110,101,116,119,111,114,107,32,100,114,105,118
  S004 db 101,32,102,111,114,32,68,79,83,32,114,117,110,110,105,110
  S005 db 103,32,111,118,101,114,32,114,97,119,32,101,116,104,101,114
  S006 db 110,101,116,13,10,13,10,85,115,97,103,101,58,32,101,116
  S007 db 104,101,114,100,102,115,32,83,82,86,77,65,67,32,114,100
  S008 db 114,105,118,101,45,108,100,114,105,118,101,32,91,111,112,116
  S009 db 105,111,110,115,93,13,10,13,10,79,112,116,105,111,110,115
  S00A db 58,13,10,32,32,47,112,61,88,88,32,32,32,117,115,101
  S00B db 32,112,97,99,107,101,116,32,100,114,105,118,101,114,32,97
  S00C db 116,32,105,110,116,101,114,114,117,112,116,32,88,88,32,40
  S00D db 97,117,116,111,100,101,116,101,99,116,32,111,116,104,101,114
  S00E db 119,105,115,101,41,13,10,32,32,47,113,32,32,32,32,32
  S00F db 32,113,117,105,101,116,32,109,111,100,101,32,40,112,114,105
  S010 db 110,116,32,110,111,116,104,105,110,103,32,105,102,32,108,111
  S011 db 97,100,101,100,32,115,117,99,99,101,115,115,102,117,108,108
  S012 db 121,41,13,10,13,10,85,115,101,32,39,58,58,39,32,97
  S013 db 115,32,83,82,86,77,65,67,32,102,111,114,32,115,101,114
  S014 db 118,101,114,32,97,117,116,111,45,100,105,115,99,111,118,101
  S015 db 114,121,46,13,10,13,10,69,120,97,109,112,108,101,115,58
  S016 db 32,32,101,116,104,101,114,100,102,115,32,54,100,58,52,102
  S017 db 58,52,97,58,52,100,58,52,57,58,53,50,32,67,45,70
  S018 db 32,47,113,13,10,32,32,32,32,32,32,32,32,32,32,32
  S019 db 101,116,104,101,114,100,102,115,32,58,58,32,68,45,88,32
  S01A db 47,112,61,54,70,13,10,'$'
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