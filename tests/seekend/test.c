/*
 * test program, part of the EtherDFS project.
 * Copyright (C) 2017 Mateusz Viste
 *
 * open a file as read-only and tries to read last 3 bytes of it. the file is
 * being opened with a 'public' access mode. the goal here is to force DOS
 * to emit a 2Fh,21h call ('seek from end of remote file').
 */


int main(int argc, char **argv) {
  unsigned short fhandle = 0;
  unsigned short errflag = 0;
  static char *fname, *msg, buff[8];

  if (argc != 2) {
    msg = "tries to read last 3 byte of a file\r\nusage: test file\r\n$";
    _asm {
      mov ah, 9
      mov dx, msg
      int 21h
    }
    return(1);
  }
  fname = argv[1];

  _asm {

    /* open file */
    mov ah, 3dh       /* open file */
    mov al, 1000000b  /* access mode: 'read-only + full access to others' */
    mov dx, fname /* DS:DX contains the ASCIZ file name */
    int 21h
    mov errflag, 1
    jc done
    mov fhandle, ax

    /* move file pointer to end of file */
    mov ax, 4202h   /* move file pointer from end of file */
    mov bx, fhandle
    xor cx, -1 /* how many bytes (hi word) */
    mov dx, -3 /* how many bytes (lo word) */
    int 21h
    mov errflag, 2
    jc done

    /* read from file */
    mov ah, 3fh
    mov bx, fhandle
    mov cx, 4  /* try to read 4 bytes (only 3 should succeed) */
    mov dx, offset buff /* DS:DX -> buffer to read to */
    int 21h
    mov errflag, 3
    jc done
    /* ax should be '3' */
    mov errflag, 4
    dec ax
    dec ax
    dec ax
    jnz done

    mov errflag, 0
    done:

    /* close file */
    mov ah, 3eh /* close file */
    mov bx, fhandle
    int 21h
  }

  if (errflag != 0) {
    msg = "ERROR 0\r\n$";
    msg[6] += errflag;
  } else {
    int i, y = 0;
    msg = "LAST 3 BYTES: ..h ..h ..h\r\n$";
    for (i = 14; i < 23; i += 4) {
      msg[i] = buff[y] >> 4;
      if (msg[i] < 10) {
        msg[i] += '0';
      } else {
        msg[i] += 'A' - 10;
      }
      msg[i+1] = buff[y++] & 15;
      if (msg[i+1] < 10) {
        msg[i+1] += '0';
      } else {
        msg[i+1] += 'A' - 10;
      }
    }
  }
  _asm {
    mov ah, 9
    mov dx, msg
    int 21h
  }

  return(errflag);
}
