/*
 * prints the content of the current directory
 */

#include <dos.h>
#include <stdio.h>

/* make sure to pack structs tightly */
#pragma pack(1)

struct sdta {
  unsigned char srch_drive;
  unsigned char srch_name[11];
  unsigned char srch_attr;
  unsigned short dir_entry;
  unsigned short start_clstr;
  unsigned short reserved;
  unsigned short start_clstr_dir;
  unsigned char fattr;
  unsigned short ftime;
  unsigned short fdate;
  unsigned long fsize;
  char fname[13];
};

static char *dostime2string(unsigned short d, unsigned short t) {
  static char buff[32];
  int year, month, day, hour, minute, second;
  year = (d >> 9) + 1980;
  month = (d >> 5) & 15;
  day = (d & 31);
  hour = (t >> 11);
  minute = (t >> 5) & 63;
  second = (t & 31) << 1;
  sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
  return(buff);
}

static void dumpdta(struct sdta far *dtaptr) {
  printf("found file: '%s'\r\n", dtaptr->fname);
  printf("fsize     : %lu bytes\r\n", dtaptr->fsize);
  printf("attribs   : %02Xh\r\n", dtaptr->fattr);
  printf("timestamp : %s\r\n", dostime2string(dtaptr->fdate, dtaptr->ftime));
  printf("dir entry : %02Xh\r\n", dtaptr->dir_entry);
  printf("search drv: %c: (%s)\r\n", 'A' + (dtaptr->srch_drive & 127), (dtaptr->srch_drive & 128)?"REMOTE":"LOCAL");
  printf("------------\r\n");
}

int main(int argc, char **argv) {
  char *buffptr;
  unsigned short cflag = 0, myax = 0;
  unsigned short dtaseg = 0, dtaoff = 0;
  struct sdta far *dtaptr;

  if (argc != 1) {
    printf("prints the content of the current directory.\r\n\r\nusage: test\r\n");
    return(1);
  }
  buffptr = "????????.???";

  _asm {
    /* get DTA address first */
    mov ah, 2Fh
    int 21h  /* DTA pointer in ES:BX */
    mov dtaseg, es
    mov dtaoff, bx
    /* perform the volume lookup */
    mov ah, 4Eh  /* FindFirst */
    mov cx, 7Fh
    mov dx, buffptr
    int 21h
    mov myax, ax
    jnc allgood
    mov cflag, 1
    allgood:
  }
  dtaptr = MK_FP(dtaseg, dtaoff);
  printf("Result: CF=%d AX=%02Xh DTA=%04X:%04X\r\n", cflag, myax, dtaseg, dtaoff);
  if (cflag != 0) return(1);
  dumpdta(dtaptr);

  for (;;) {
    _asm {
      mov ah, 4Fh  /* FindNext */
      int 21h
      mov myax, ax
      jnc allgood
      mov cflag, 1
      allgood:
    }
    printf("Result: CF=%d AX=%02Xh DTA=%04X:%04X\r\n", cflag, myax, dtaseg, dtaoff);
    if (cflag != 0) return(1);
    dumpdta(dtaptr);
  }

  return(0);
}
