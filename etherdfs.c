/*
 * ethdrive - a network drive for DOS running over raw ethernet
 * http://etherdfs.sourceforge.net
 *
 * Copyright (C) 2017 Mateusz Viste
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <i86.h>     /* union INTPACK */
#include "chint.h"   /* _mvchain_intr() */
#include "version.h" /* program & protocol version */

/* set DEBUGLEVEL to 0, 1 or 2 to turn on debug mode with desired verbosity */
#define DEBUGLEVEL 0

/* define the maximum size of a frame, as sent or received by etherdfs.
 * example: value 1084 accomodates payloads up to 1024 bytes +all headers */
#define FRAMESIZE 1090

#include "dosstruc.h" /* definitions of structures used by DOS */
#include "globals.h"  /* global variables used by etherdfs */

/* define NULL, for readability of the code */
#ifndef NULL
  #define NULL (void *)0
#endif

/* all the resident code goes to segment 'BEGTEXT' */
#pragma code_seg(BEGTEXT, CODE)


/* copies l bytes from *s to *d */
static void copybytes(void far *d, void far *s, unsigned int l) {
  for (;;) {
    if (l == 0) break;
    l--;
    *(unsigned char far *)d = *(unsigned char far *)s;
    d = (unsigned char far *)d + 1;
    s = (unsigned char far *)s + 1;
  }
}

/* returns -1 if the NULL-terminated s string contains any wildcard (?, *)
 * character. otherwise returns the length of the string. */
static int len_if_no_wildcards(char far *s) {
  int r = 0;
  for (;;) {
    switch (*s) {
      case 0: return(r);
      case '?':
      case '*': return(-1);
    }
    r++;
    s++;
  }
}

/* this function is called two times by the packet driver. One time for
 * telling that a packet is incoming, and how big it is, so the application
 * can prepare a buffer for it and hand it back to the packet driver. the
 * second call is just let know that the frame has been copied into the
 * buffer. This is a naked function - I don't need the compiler to get into
 * the way when dealing with packet driver callbacks. */
void __declspec(naked) far pktdrv_recv(void) {
  _asm {
    jmp skip
    SIG db 'p','k','t','r','c','v'
    skip:
    /* set my custom DS (not 0, it has been patched at runtime already) */
    push ax
    mov ax, 0
    mov ds, ax
    pop ax
    /* handle the call */
    cmp ax, 0
    jne secondcall /* if ax > 0, then packet driver just filled my buffer */
    /* first call: the packet driver needs a buffer of CX bytes */
    cmp cx, FRAMESIZE /* is cx > FRAMESIZE ? (unsigned) */
    ja nobufferavail  /* it is too small (that's what she said!) */
    /* see if buffer not filled already... */
    cmp glob_pktdrv_recvbufflen, 0 /* is bufflen > 0 ? (signed) */
    jg nobufferavail  /* if signed > 0, then we are busy already */

    /* buffer is available, set its seg:off in es:di */
    push ds /* set es:di to recvbuff */
    pop es
    mov di, offset glob_pktdrv_recvbuff
    /* set bufferlen to expected len and switch it to neg until data comes */
    mov glob_pktdrv_recvbufflen, cx
    neg glob_pktdrv_recvbufflen
    retf

  nobufferavail: /* no buffer available, or it's too small -> fail */
    push ax        /* save ax */
    xor ax,ax      /* set ax to zero... */
    push ax        /* and push it to the stack... */
    push ax        /* twice */
    pop es         /* zero out es and di - this tells the */
    pop di         /* packet driver 'sorry no can do'     */
    pop ax         /* restore ax */
    retf

  secondcall: /* second call: I've just got data in buff */
    /* I switch back bufflen to positive so the app can see that something is there now */
    neg glob_pktdrv_recvbufflen
    retf
  }
}


static void pktdrv_send(void *buffptr, unsigned short bufferlen) {
  _asm {
    mov ah, 4h  /* SendPkt */
    mov cx, bufferlen
    push si
    mov si, buffptr /* DS:SI should point to the buff, I do not modify DS
                     * because I assume the buffer is already in my data
                     * segment (small memory model) */
    /* int to variable vector is a mess, so I have fetched its vector myself
     * and pushf + cli + call far it now to simulate a regular int */
    pushf
    cli
    call dword ptr glob_pktdrv_pktcall
    /* restore SI */
    pop si
  }

#if DEBUGLEVEL > 1
  { /* This block is for DEBUG purpose only - print the frame that is about to be sent on screen */
    int i;
    for (i = 0; i < 26; i++) {
      dbg_VGA[240+i*3] = 0x1700 | dbg_hexc[((unsigned char *)buffptr)[i] >> 4];
      dbg_VGA[241+i*3] = 0x1700 | dbg_hexc[((unsigned char *)buffptr)[i] & 15];
      dbg_VGA[242+i*3] = 0x1700;
    }
    if (regs.x.cflag != 0) {
      dbg_VGA[dbg_startoffset+dbg_xpos++] = 0x8100 | '!';
      dbg_VGA[dbg_startoffset+dbg_xpos++] = 0x8100 | dbg_hexc[regs.h.dh >> 4];
      dbg_VGA[dbg_startoffset+dbg_xpos++] = 0x8100 | dbg_hexc[regs.h.dh & 15];
    } else {
      dbg_VGA[dbg_startoffset+dbg_xpos++] = 0x8100 | '+';
    }
  }
#endif
}


/* translates a drive letter (either upper- or lower-case) into a number (A=0,
 * B=1, C=2, etc) */
#define DRIVETONUM(x) (((x) >= 'a') && ((x) <= 'z')?x-'a':x-'A')


/* all the calls I support are in the range AL=0..2Eh - the list below serves
 * as a convenience to compare AL (subfunction) values */
enum AL_SUBFUNCTIONS {
  AL_INSTALLCHK = 0x00,
  AL_RMDIR      = 0x01,
  AL_MKDIR      = 0x03,
  AL_CHDIR      = 0x05,
  AL_CLSFIL     = 0x06,
  AL_CMMTFIL    = 0x07,
  AL_READFIL    = 0x08,
  AL_WRITEFIL   = 0x09,
  AL_LOCKFIL    = 0x0A,
  AL_UNLOCKFIL  = 0x0B,
  AL_DISKSPACE  = 0x0C,
  AL_SETATTR    = 0x0E,
  AL_GETATTR    = 0x0F,
  AL_RENAME     = 0x11,
  AL_DELETE     = 0x13,
  AL_OPEN       = 0x16,
  AL_CREATE     = 0x17,
  AL_FINDFIRST  = 0x1B,
  AL_FINDNEXT   = 0x1C,
  AL_SKFMEND    = 0x21,
  AL_UNKNOWN_2D = 0x2D,
  AL_SPOPNFIL   = 0x2E,
  AL_UNKNOWN    = 0xFF
};

/* this table makes it easy to figure out if I want a subfunction or not */
static unsigned char supportedfunctions[0x2F] = {
  AL_INSTALLCHK,  /* 0x00 */
  AL_RMDIR,       /* 0x01 */
  AL_UNKNOWN,     /* 0x02 */
  AL_MKDIR,       /* 0x03 */
  AL_UNKNOWN,     /* 0x04 */
  AL_CHDIR,       /* 0x05 */
  AL_CLSFIL,      /* 0x06 */
  AL_CMMTFIL,     /* 0x07 */
  AL_READFIL,     /* 0x08 */
  AL_WRITEFIL,    /* 0x09 */
  AL_LOCKFIL,     /* 0x0A */
  AL_UNLOCKFIL,   /* 0x0B */
  AL_DISKSPACE,   /* 0x0C */
  AL_UNKNOWN,     /* 0x0D */
  AL_SETATTR,     /* 0x0E */
  AL_GETATTR,     /* 0x0F */
  AL_UNKNOWN,     /* 0x10 */
  AL_RENAME,      /* 0x11 */
  AL_UNKNOWN,     /* 0x12 */
  AL_DELETE,      /* 0x13 */
  AL_UNKNOWN,     /* 0x14 */
  AL_UNKNOWN,     /* 0x15 */
  AL_OPEN,        /* 0x16 */
  AL_CREATE,      /* 0x17 */
  AL_UNKNOWN,     /* 0x18 */
  AL_UNKNOWN,     /* 0x19 */
  AL_UNKNOWN,     /* 0x1A */
  AL_FINDFIRST,   /* 0x1B */
  AL_FINDNEXT,    /* 0x1C */
  AL_UNKNOWN,     /* 0x1D */
  AL_UNKNOWN,     /* 0x1E */
  AL_UNKNOWN,     /* 0x1F */
  AL_UNKNOWN,     /* 0x20 */
  AL_SKFMEND,     /* 0x21 */
  AL_UNKNOWN,     /* 0x22 */
  AL_UNKNOWN,     /* 0x23 */
  AL_UNKNOWN,     /* 0x24 */
  AL_UNKNOWN,     /* 0x25 */
  AL_UNKNOWN,     /* 0x26 */
  AL_UNKNOWN,     /* 0x27 */
  AL_UNKNOWN,     /* 0x28 */
  AL_UNKNOWN,     /* 0x29 */
  AL_UNKNOWN,     /* 0x2A */
  AL_UNKNOWN,     /* 0x2B */
  AL_UNKNOWN,     /* 0x2C */
  AL_UNKNOWN_2D,  /* 0x2D */
  AL_SPOPNFIL     /* 0x2E */
};

/*
an INTPACK struct contains following items:
regs.w.gs
regs.w.fs
regs.w.es
regs.w.ds
regs.w.di
regs.w.si
regs.w.bp
regs.w.sp
regs.w.bx
regs.w.dx
regs.w.cx
regs.w.ax
regs.w.ip
regs.w.cs
regs.w.flags (AND with INTR_CF to fetch the CF flag - INTR_CF is defined as 0x0001)

regs.h.bl
regs.h.bh
regs.h.dl
regs.h.dh
regs.h.cl
regs.h.ch
regs.h.al
regs.h.ah
*/


/* This function should not be necessary. DOS usually generates an FCB
 * style name in the appropriate SDA area. However, in the case of
 * user input such as 'CD ..' or 'DIR ..' it leaves the fcb area all
 * spaces. So this function needs to be called every time. */
static void gen_fcbname_from_path(void) {
  int i;
  unsigned char far *path = glob_sdaptr->fn1;

  /* fast forward 'path' to first character of the filename */
  for (i = 0;; i++) {
    if (glob_sdaptr->fn1[i] == '\\') path = glob_sdaptr->fn1 + i + 1;
    if (glob_sdaptr->fn1[i] == 0) break;
  }

  /* clear out fcb_fn1 by filling it with spaces */
  for (i = 0; i < 11; i++) glob_sdaptr->fcb_fn1[i] = ' ';

  /* copy 'path' into fcb_name using the fcb syntax ("FILE    TXT") */
  for (i = 0; *path != 0; path++) {
    if (*path == '.') {
      i = 8;
    } else {
      glob_sdaptr->fcb_fn1[i++] = *path;
    }
  }
}


/* sends query out, as found in glob_pktdrv_sndbuff, and awaits for an answer.
 * this function returns the length of replyptr, or 0xFFFF on error. */
static unsigned short sendquery(unsigned char query, unsigned char drive, unsigned short bufflen, unsigned char **replyptr, unsigned short **replyax, unsigned int updatermac) {
  static unsigned char seq;
  unsigned short count;
  unsigned char t;
  unsigned char volatile far *rtc = (unsigned char far *)0x46C; /* this points to a char, while the rtc timer is a word - but I care only about the lowest 8 bits. Be warned that this location won't increment while interrupts are disabled! */

  /* resolve remote drive or die (for now it's easy since only a single drive
   * can possibly be configured, but future versions shall support multiple
   * drive mappings */
  if (drive == glob_ldrv) {
    drive = glob_rdrv;
  } else { /* drive not found */
    return(0);
  }
  /* if query too long then quit */
  if (bufflen > (sizeof(glob_pktdrv_sndbuff) - 60)) return(0);
  /* inc seq */
  seq++;
  /* */
  copybytes((unsigned char far *)glob_pktdrv_sndbuff, (unsigned char far *)glob_rmac, 6);
  copybytes((unsigned char far *)glob_pktdrv_sndbuff + 6, (unsigned char far *)glob_lmac, 6);
  glob_pktdrv_sndbuff[12] = 0xED;
  glob_pktdrv_sndbuff[13] = 0xF5;
  /* padding (42 bytes) */
  glob_pktdrv_sndbuff[56] = PROTOVER; /* protocol version */
  glob_pktdrv_sndbuff[57] = seq;   /* seq number */
  glob_pktdrv_sndbuff[58] = drive; /* this should be resolved to remote drive */
  glob_pktdrv_sndbuff[59] = query; /* AL value (query) */
  /* I do not copy anything more into glob_pktdrv_sndbuff - the caller is
   * expected to have already copied all relevant data into glob_pktdrv_sndbuff+60
   * copybytes((unsigned char far *)glob_pktdrv_sndbuff + 60, (unsigned char far *)buff, bufflen);
   */

  /* send the query frame and wait for an answer for about 100ms. then, resend
   * the query again and again, up to 3 times. the RTC clock at 0x46C is used
   * as a timing reference. */
  glob_pktdrv_recvbufflen = 0; /* mark the receiving buffer empty */
  for (count = 0; count < 3; count++) {
    /* send the query frame out */
    pktdrv_send(glob_pktdrv_sndbuff, bufflen + 60);
    t = *rtc;

    /* wait for (and validate) the answer frame */
    for (;;) {
      int i;
      if ((t != *rtc) && (t+1 != *rtc) && (*rtc != 0)) break; /* timeout, retry */
      if (glob_pktdrv_recvbufflen < 1) continue;
      /* I've got something! */
      /* is the frame long enough for me to care? */
      if (glob_pktdrv_recvbufflen < 60) goto ignoreframe;
      /* is it for me? (correct src mac & dst mac) */
      for (i = 0; i < 6; i++) {
        if (glob_pktdrv_recvbuff[i] != glob_lmac[i]) goto ignoreframe;
        if ((updatermac == 0) && (glob_pktdrv_recvbuff[i+6] != glob_rmac[i])) goto ignoreframe;
      }
      /* is the ethertype and seq what I expect? */
      if ((glob_pktdrv_recvbuff[12] != 0xED) || (glob_pktdrv_recvbuff[13] != 0xF5) || (glob_pktdrv_recvbuff[57] != glob_pktdrv_sndbuff[57])) goto ignoreframe;
      /* return buffer (without headers and seq) */
      *replyptr = glob_pktdrv_recvbuff + 60;
      *replyax = (unsigned short *)(glob_pktdrv_recvbuff + 58);
      /* update glob_rmac if needed, then return */
      if (updatermac != 0) copybytes(glob_rmac, glob_pktdrv_recvbuff + 6, 6);
      return(glob_pktdrv_recvbufflen - 60);
      ignoreframe: /* ignore this frame and wait for the next one */
      glob_pktdrv_recvbufflen = 0; /* mark the buffer empty */
    }
  }
  return(0xFFFFu); /* return error */
}


/* reset CF (set on error only) and AX (expected to contain the error code,
 * I might set it later) - I assume a success */
#define SUCCESSFLAG glob_intregs.w.ax = 0; glob_intregs.w.flags &= ~(INTR_CF);
#define FAILFLAG(x) glob_intregs.w.ax = x; glob_intregs.w.flags |= INTR_CF;

/* this function contains the logic behind INT 2F processing */
void process2f(void) {
#if DEBUGLEVEL > 0
  char far *dbg_msg = NULL;
#endif
  short i;
  unsigned char *answer;
  unsigned char *buff; /* pointer to the "query arguments" part of glob_pktdrv_sndbuff */
  unsigned char subfunction;
  unsigned short *ax; /* used to collect the resulting value of AX */
  buff = glob_pktdrv_sndbuff + 60;

  /* DEBUG output (RED) */
#if DEBUGLEVEL > 0
  dbg_xpos &= 511;
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x4e00 | ' ';
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x4e00 | (dbg_hexc[(glob_intregs.h.al >> 4) & 0xf]);
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x4e00 | (dbg_hexc[glob_intregs.h.al & 0xf]);
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x4e00 | ' ';
#endif

  /* remember the AL register (0x2F subfunction id) */
  subfunction = glob_intregs.h.al;

  /* if we got here, then the call is definitely for us. set AX and CF to */
  /* 'success' (being a natural optimist I assume success) */
  SUCCESSFLAG;

  /* look what function is called exactly and process it */
  switch (subfunction) {
    case AL_RMDIR: /*** 01h: RMDIR ******************************************/
      /* RMDIR is like MKDIR, but I need to check if dir is not current first */
      for (i = 0; glob_sdaptr->fn1[i] != 0; i++) {
        if (glob_sdaptr->fn1[i] != glob_sdaptr->drive_cdsptr[i]) break;
      }
      if (glob_sdaptr->fn1[i] == 0) {
        FAILFLAG(16); /* err 16 = "attempted to remove current directory" */
        break;
      }
    case AL_MKDIR: /*** 03h: MKDIR ******************************************/
      /* compute the length of fn1 and copy it to buff */
      for (i = 2; glob_sdaptr->fn1[i] != 0; i++) buff[i - 2] = glob_sdaptr->fn1[i];
      /* send query providing fn1 (but skip the first two bytes with drive) */
      if (sendquery(subfunction, glob_reqdrv, i - 2, &answer, &ax, 0) == 0) {
        glob_intregs.w.ax = *ax;
        if (*ax != 0) glob_intregs.w.flags |= INTR_CF;
      } else {
        FAILFLAG(2);
      }
      break;
    case AL_CHDIR: /*** 05h: CHDIR ******************************************/
      /* The INT 2Fh/1105h redirector callback is executed by DOS when
       * changing directories. The Phantom authors (and RBIL contributors)
       * clearly thought that it was the redirector's job to update the CDS,
       * but in fact the callback is only meant to validate that the target
       * directory exists; DOS subsequently updates the CDS. */
      /* compute the length of fn1 and copy it to buff */
      for (i = 0; glob_sdaptr->fn1[i] != 0; i++);
      if (i < 2) {
        FAILFLAG(3); /* "path not found" */
        break;
      }
      i -= 2;
      copybytes(buff, glob_sdaptr->fn1 + 2, i);
      /* send query providing fn1 (but skip the drive: part) */
      if (sendquery(AL_CHDIR, glob_reqdrv, i, &answer, &ax, 0) == 0) {
        glob_intregs.w.ax = *ax;
        if (*ax != 0) glob_intregs.w.flags |= INTR_CF;
      } else {
        FAILFLAG(3); /* "path not found" */
      }
      break;
    case AL_CLSFIL: /*** 06h: CLSFIL ****************************************/
      /* my only job is to decrement the SFT's handle count (which I didn't
       * have to increment during OPENFILE since DOS does it... talk about
       * consistency. I also inform the server about this, just so it knows */
      /* ES:DI points to the SFT */
      {
      struct sftstruct far *sftptr = MK_FP(glob_intregs.x.es, glob_intregs.x.di);
      sftptr->handle_count--;
      ((unsigned short *)buff)[0] = sftptr->start_sector;
      if (sendquery(AL_CLSFIL, glob_reqdrv, 2, &answer, &ax, 0) == 0) {
        if (*ax != 0) FAILFLAG(*ax);
      }
      }
      break;
    case AL_CMMTFIL: /*** 07h: CMMTFIL **************************************/
      /* I have nothing to do here */
      break;
    case AL_READFIL: /*** 08h: READFIL **************************************/
      { /* ES:DI points to the SFT (whose file_pos needs to be updated) */
        /* CX = number of bytes to read (to be updated with number of bytes actually read) */
        /* SDA DTA = read buffer */
      struct sftstruct far *sftptr = MK_FP(glob_intregs.x.es, glob_intregs.x.di);
      unsigned short totreadlen;
      /* is the file open for write-only? */
      if (sftptr->open_mode & 1) {
        FAILFLAG(5); /* "access denied" */
        break;
      }
      /* return immediately if the caller wants to read 0 bytes */
      if (glob_intregs.x.cx == 0) break;
      /* do multiple read operations so chunks can fit in my eth frames */
      totreadlen = 0;
      for (;;) {
        int chunklen, len;
        if ((glob_intregs.x.cx - totreadlen) < (FRAMESIZE - 60)) {
          chunklen = glob_intregs.x.cx - totreadlen;
        } else {
          chunklen = FRAMESIZE - 60;
        }
        /* query is SSPP (start sector, file position) */
        ((unsigned long *)buff)[0] = sftptr->file_pos + totreadlen;
        ((unsigned short *)buff)[2] = sftptr->start_sector;
        ((unsigned short *)buff)[3] = chunklen;
        len = sendquery(AL_READFIL, glob_reqdrv, 8, &answer, &ax, 0);
        if (len == 0xFFFFu) { /* network error */
          FAILFLAG(2);
          break;
        } else if (*ax != 0) { /* backend error */
          FAILFLAG(*ax);
          break;
        } else { /* success */
          copybytes(glob_sdaptr->curr_dta + totreadlen, answer, len);
          totreadlen += len;
          if ((len < chunklen) || (totreadlen == glob_intregs.x.cx)) { /* EOF - update SFT and break out */
            sftptr->file_pos += totreadlen;
            glob_intregs.x.cx = totreadlen;
            break;
          }
        }
      }
      }
      break;
    case AL_WRITEFIL: /*** 09h: WRITEFIL ************************************/
      { /* ES:DI points to the SFT (whose file_pos needs to be updated) */
        /* CX = number of bytes to read (to be updated with number of bytes actually read) */
        /* SDA DTA = read buffer */
      struct sftstruct far *sftptr = MK_FP(glob_intregs.x.es, glob_intregs.x.di);
      unsigned short bytesleft, chunklen, written = 0;
      /* is the file open for read-only? */
      if ((sftptr->open_mode & 3) == 0) {
        FAILFLAG(5); /* "access denied" */
        break;
      }
      /* TODO FIXME I should update the file's time in the SFT here */
      /* do multiple write operations so chunks can fit in my eth frames */
      bytesleft = glob_intregs.x.cx;

      while (bytesleft > 0) {
        unsigned short len;
        chunklen = bytesleft;
        if (chunklen > FRAMESIZE - 66) chunklen = FRAMESIZE - 66;
        /* query is OOOOSS (file offset, start sector/fileid) */
        ((unsigned long *)buff)[0] = sftptr->file_pos;
        ((unsigned short *)buff)[2] = sftptr->start_sector;
        copybytes(buff + 6, glob_sdaptr->curr_dta + written, chunklen);
        len = sendquery(AL_WRITEFIL, glob_reqdrv, chunklen + 6, &answer, &ax, 0);
        if (len == 0xFFFFu) { /* network error */
          FAILFLAG(2);
          break;
        } else if ((*ax != 0) || (len != 2)) { /* backend error */
          FAILFLAG(*ax);
          break;
        } else { /* success - write amount of bytes written into CX and update SFT */
          len = ((unsigned short *)answer)[0];
          written += len;
          bytesleft -= len;
          glob_intregs.x.cx = written;
          sftptr->file_pos += len;
          if (sftptr->file_pos > sftptr->file_size) sftptr->file_size = sftptr->file_pos;
          if (len < chunklen) break; /* something bad happened on the other side */
        }
      }

      }
      break;
    case AL_LOCKFIL: /*** 0Ah: LOCKFIL **************************************/
      /* TODO */
      FAILFLAG(2);
      break;
    case AL_UNLOCKFIL: /*** 0Bh: UNLOCKFIL **********************************/
      /* TODO */
      FAILFLAG(2);
      break;
    case AL_DISKSPACE: /*** 0Ch: get disk information ***********************/
      if (sendquery(AL_DISKSPACE, glob_reqdrv, 0, &answer, &ax, 0) == 6) {
        glob_intregs.w.ax = *ax; /* sectors per cluster */
        glob_intregs.w.bx = ((unsigned short *)answer)[0]; /* total clusters */
        glob_intregs.w.cx = ((unsigned short *)answer)[1]; /* bytes per sector */
        glob_intregs.w.dx = ((unsigned short *)answer)[2]; /* num of available clusters */
      } else {
        FAILFLAG(2);
      }
      break;
    case AL_SETATTR: /*** 0Eh: SETATTR **************************************/
      /* TODO */
      FAILFLAG(2);
      break;
    case AL_GETATTR: /*** 0Fh: GETATTR **************************************/
      for (i = 0; glob_sdaptr->fn1[i] != 0; i++); /* strlen(fn1) */
      if (i < 2) {
        FAILFLAG(2);
        break;
      }
      i -= 2;
      copybytes(buff, glob_sdaptr->fn1 + 2, i);
      i = sendquery(AL_GETATTR, glob_reqdrv, i, &answer, &ax, 0);
      if ((unsigned short)i == 0xffffu) {
        FAILFLAG(2);
      } else if ((i != 9) || (*ax != 0)) {
        FAILFLAG(*ax);
      } else { /* all good */
        /* CX = timestamp
         * DX = datestamp
         * BX:DI = fsize
         * AX = attr
         * NOTE: Undocumented DOS talks only about setting AX, no fsize, time
         *       and date, these are documented in RBIL and used by SHSUCDX */
        glob_intregs.w.cx = ((unsigned short *)answer)[0]; /* time */
        glob_intregs.w.dx = ((unsigned short *)answer)[1]; /* date */
        glob_intregs.w.bx = ((unsigned short *)answer)[3]; /* fsize hi word */
        glob_intregs.w.di = ((unsigned short *)answer)[2]; /* fsize lo word */
        glob_intregs.w.ax = answer[8];                     /* file attribs */
      }
      break;
    case AL_RENAME: /*** 11h: RENAME ****************************************/
      /* sdaptr->fn1 = old name
       * sdaptr->fn2 = new name */
      /* is the operation for the SAME drive? */
      if (glob_sdaptr->fn1[0] != glob_sdaptr->fn2[0]) {
        FAILFLAG(2);
        break;
      }
      /* prepare the query (LSSS...DDD...) */
      for (i = 0; glob_sdaptr->fn1[i] != 0; i++); /* strlen(fn1) */
      if (i < 2) {
        FAILFLAG(2);
        break;
      }
      i -= 2; /* trim out the drive: part (C:\FILE --> \FILE) */
      buff[0] = i;
      copybytes(buff + 1, glob_sdaptr->fn1 + 2, i);
      for (i = 0; glob_sdaptr->fn2[i] != 0; i++); /* strlen(fn2) */
      if (i < 2) {
        FAILFLAG(2);
        break;
      }
      i -= 2; /* trim out the drive: part (C:\FILE --> \FILE) */
      copybytes(buff + 1 + buff[0], glob_sdaptr->fn2 + 2, i);
      /* send the query out */
      i = sendquery(AL_RENAME, glob_reqdrv, 1 + buff[0] + i, &answer, &ax, 0);
      if (i != 0) {
        FAILFLAG(2);
      } else if (*ax != 0) {
        FAILFLAG(*ax);
      }
      break;
    case AL_DELETE: /*** 13h: DELETE ****************************************/
    #if DEBUGLEVEL > 0
      dbg_msg = glob_sdaptr->fn1;
    #endif
      /* compute length of fn1 and copy it to buff (w/o the 'drive:' part) */
      for (i = 0; glob_sdaptr->fn1[i] != 0; i++);
      if (i < 2) {
        FAILFLAG(2);
        break;
      }
      i -= 2;
      copybytes(buff, glob_sdaptr->fn1 + 2, i);
      /* send query */
      i = sendquery(AL_DELETE, glob_reqdrv, i, &answer, &ax, 0);
      if ((unsigned short)i == 0xffffu) {
        FAILFLAG(2);
      } else if ((i != 0) || (*ax != 0)) {
        FAILFLAG(*ax);
      }
      break;
    case AL_OPEN: /*** 16h: OPEN ********************************************/
    case AL_CREATE: /*** 17h: CREATE ****************************************/
    #if DEBUGLEVEL > 0
      dbg_msg = glob_sdaptr->fn1;
    #endif
      /* fail if fn1 contains any wildcard, otherwise get len of fn1 */
      i = len_if_no_wildcards(glob_sdaptr->fn1);
      if (i < 2) {
        FAILFLAG(3);
        break;
      }
      i -= 2;
      /* send query */
      copybytes(buff, glob_sdaptr->fn1 + 2, i);
      i = sendquery(subfunction, glob_reqdrv, i, &answer, &ax, 0);
      if ((unsigned short)i == 0xffffu) {
        FAILFLAG(2);
      } else if ((i != 22) || (*ax != 0)) {
        FAILFLAG(*ax);
      } else {
        /* ES:DI contains an uninitialized SFT */
        struct sftstruct far *sftptr = MK_FP(glob_intregs.x.es, glob_intregs.x.di);
        sftptr->open_mode &= 0xfff0; /* sanitize the open mode */
        sftptr->open_mode |= 2; /* read/write */
        if (sftptr->open_mode & 0x8000) {
          /* TODO FIXME no idea what I should do here */
          /* set_sft_owner(p); */
        #if DEBUGLEVEL > 0
          dbg_VGA[25*80] = 0x1700 | '$';
        #endif
        }
        sftptr->file_attr = answer[0];
        sftptr->dev_info_word = 0x8040 | glob_reqdrv; /* mark device as network drive */
        sftptr->dev_drvr_ptr = NULL;
        sftptr->start_sector = ((unsigned short *)answer)[10];
        sftptr->file_time = ((unsigned long *)answer)[3];
        sftptr->file_size = ((unsigned long *)answer)[4];
        sftptr->file_pos = 0;
        sftptr->rel_sector = 0xffff;
        sftptr->abs_sector = 0xffff;
        sftptr->dir_sector = 0;
        sftptr->dir_entry_no = 0;
        copybytes(sftptr->file_name, answer + 1, 11);
      }
      break;
    case AL_FINDFIRST: /*** 1Bh: FINDFIRST **********************************/
    case AL_FINDNEXT:  /*** 1Bh: FINDNEXT ***********************************/
      {
      /* AX = 111Bh
      SS = DS = DOS DS
      [DTA] = uninitialized 21-byte findfirst search data
      (see #01626 at INT 21/AH=4Eh)
      SDA first filename pointer (FN1, 9Eh) -> fully-qualified search template
      SDA CDS pointer -> current directory structure for drive with file
      SDA search attribute = attribute mask for search

      Return:
      CF set on error
      AX = DOS error code (see #01680 at INT 21/AH=59h/BX=0000h)
           -> http://www.ctyme.com/intr/rb-3012.htm
      CF clear if successful
      [DTA] = updated findfirst search data
      (bit 7 of first byte must be set)
      [DTA+15h] = standard directory entry for file (see #01352) */

#if DEBUGLEVEL > 0
      dbg_msg = glob_sdaptr->fn1;
#endif
      /* prepare the query buffer */
      if (subfunction == AL_FINDFIRST) {
        ((unsigned short far *)buff)[0] = 0; /* unused (for findnext only) */
      } else { /* FindNext needs to increment dir_entry */
        glob_sdaptr->sdb.dir_entry++;
        ((unsigned short far *)buff)[0] = glob_sdaptr->sdb.dir_entry;
      }
      buff[2] = glob_sdaptr->srch_attr; /* file attributes to look for */
      /* copy fn1 (w/o drive) to buff */
      for (i = 2; glob_sdaptr->fn1[i] != 0; i++) buff[i+1] = glob_sdaptr->fn1[i];
      /* send query to remote peer and wait for answer */
      if (sendquery(subfunction, glob_reqdrv, i+1, &answer, &ax, 0) != 20) {
        if (subfunction == AL_FINDFIRST) {
          FAILFLAG(2); /* a failed findfirst returns error 2 (file not found) */
        } else {
          FAILFLAG(18); /* a failed findnext returns error 18 (no more files) */
        }
        break;
      } else if (*ax != 0) {
        FAILFLAG(*ax);
        break;
      }
      /* fill in the directory entry 'found_file' (32 bytes)
       * unsigned char fname[11]
       * unsigned char fattr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEV)
       * unsigned char f1[10]
       * unsigned short time_lstupd
       * unsigned short date_lstupd
       * unsigned short start_clstr  *optional*
       * unsigned long fsize
       */
      copybytes(glob_sdaptr->found_file.fname, answer+1, 11); /* found file name */
      glob_sdaptr->found_file.fattr = answer[0]; /* found file attributes */
      glob_sdaptr->found_file.time_lstupd = ((unsigned short *)answer)[6]; /* time (word) */
      glob_sdaptr->found_file.date_lstupd = ((unsigned short *)answer)[7]; /* date (word) */
      glob_sdaptr->found_file.start_clstr = 0; /* start cluster (I don't care) */
      glob_sdaptr->found_file.fsize = ((unsigned long *)answer)[4]; /* fsize (word) */

      /* put things into SDB so I can understand where I left should FindNext be called
       *   unsigned char drv_lett
       *   unsigned char srch_tmpl[11]
       *   unsigned char srch_attr
       *   unsigned short dir_entry
       *   unsigned short par_clstr
       *   unsigned char f1[4]
       */
      glob_sdaptr->sdb.drv_lett = glob_reqdrv;
      copybytes(glob_sdaptr->sdb.srch_tmpl, glob_sdaptr->fcb_fn1, 11);
      glob_sdaptr->sdb.srch_attr = glob_sdaptr->srch_attr;
      /* zero out dir_entry only on FindFirst (FindNext contains a valid value already) */
      if (subfunction == AL_FINDFIRST) glob_sdaptr->sdb.dir_entry = 0;
      glob_sdaptr->sdb.par_clstr = 0;

      /* update the DTA with a valid FindFirst structure (21 bytes)
       * 00h unsigned char drive letter (7bits, MSB must be set for remote drives)
       * 01h unsigned char search_tmpl[11]
       * 0Ch unsigned char search_attr (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEV)
       * 0Dh unsigned short entry_count_within_directory
       * 0Fh unsigned short cluster number of start of parent directory
       * 11h unsigned char reserved[4]
       * -- RBIL says: [DTA+15h] = standard directory entry for file
       * 15h 11-bytes (FCB-style) filename+ext ("FILE0000TXT")
       * 20h unsigned char attr. of file found (1=RO 2=HID 4=SYS 8=VOL 16=DIR 32=ARCH 64=DEV)
       * 21h 10-bytes reserved
       * 2Bh unsigned short file time
       * 2Dh unsigned short file date
       * 2Fh unsigned short cluster
       * 31h unsigned long file size
       */
      copybytes(glob_sdaptr->curr_dta, &(glob_sdaptr->sdb), 21); /* first 21 bytes as in SDB */
      glob_sdaptr->curr_dta[0] |= 128; /* bit 7 set means 'network drive' */
      /* then 32 bytes as in the found_file record */
      copybytes(glob_sdaptr->curr_dta + 0x15, &(glob_sdaptr->found_file), 32);
      }
      break;
    case AL_SKFMEND: /*** 21h: SKFMEND **************************************/
      /* TODO */
      FAILFLAG(2);
      break;
    case AL_UNKNOWN_2D: /*** 2Dh: UNKNOWN_2D ********************************/
      /* this is only called in MS-DOS v4.01, its purpose is unknown. MSCDEX
       * returns AX=2 there, and so do I. */
      glob_intregs.w.ax = 2;
      break;
    case AL_SPOPNFIL: /*** 2Eh: SPOPNFIL ************************************/
      /* TODO */
      FAILFLAG(2);
      break;
  }

  /* DEBUG */
#if DEBUGLEVEL > 0
  while ((dbg_msg != NULL) && (*dbg_msg != 0)) dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x4f00 | *(dbg_msg++);
#endif
}

/* this function is hooked on INT 2Fh */
void __interrupt __far inthandler(union INTPACK r) {
  /* insert a static code signature so I can detect later if I am loaded already,
   * this will also contain the DS segment to use and actually set it */
  _asm {
    jmp SKIPTSRSIG
    TSRSIG DB 'M','V','e','t','h','d'
    SKIPTSRSIG:
    push ax
    mov ax, 0
    mov ds, ax
    pop ax
  }

  /* DEBUG output (BLUE) */
#if DEBUGLEVEL > 1
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x1e00 | (dbg_hexc[(r.h.al >> 4) & 0xf]);
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x1e00 | (dbg_hexc[r.h.al & 0xf]);
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0;
#endif

  /* if not related to a redirector function (AH=11h), or the function is
   * an 'install check' (0), or the function is over our scope (2Eh), or it's
   * an otherwise unsupported function (as pointed out by supportedfunctions),
   * then call the previous INT 2F handler immediately */
  if ((r.h.ah != 0x11) || (r.h.al == AL_INSTALLCHK) || (r.h.al > 0x2E) || (supportedfunctions[r.h.al] == AL_UNKNOWN)) goto CHAINTOPREVHANDLER;

  /* DEBUG output (GREEN) */
#if DEBUGLEVEL > 0
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x2e00 | (dbg_hexc[(r.h.al >> 4) & 0xf]);
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x2e00 | (dbg_hexc[r.h.al & 0xf]);
  dbg_VGA[dbg_startoffset + dbg_xpos++] = 0;
#endif

  /* determine whether or not the query is meant for a drive I control,
   * and if not - chain to the previous INT 2F handler */
  if (((r.h.al >= AL_CLSFIL) && (r.h.al <= AL_UNLOCKFIL)) || (r.h.al == AL_SKFMEND) || (r.h.al == AL_UNKNOWN_2D)) {
  /* ES:DI points to the SFT: if the bottom 6 bits of the device information
   * word in the SFT are > last drive, then it relates to files not associated
   * with drives, such as LAN Manager named pipes. */
    struct sftstruct far *sft = MK_FP(r.w.es, r.w.di);
    glob_reqdrv = sft->dev_info_word & 0x3F;
  } else {
    switch (r.h.al) {
      case AL_FINDNEXT:
        glob_reqdrv = glob_sdaptr->sdb.drv_lett & 0x1F;
        break;
      case AL_GETATTR:
      case AL_DELETE:
      case AL_OPEN:
      case AL_CREATE:
      case AL_MKDIR:
      case AL_RMDIR:
      case AL_CHDIR:
      case AL_RENAME: /* check sda.fn1 for drive */
        glob_reqdrv = DRIVETONUM(glob_sdaptr->fn1[0]);
        break;
      default: /* otherwise check out the CDS (at ES:DI) */
        {
        struct cdsstruct far *cds = MK_FP(r.w.es, r.w.di);
        glob_reqdrv = DRIVETONUM(cds->current_path[0]);
      #if DEBUGLEVEL > 0 /* DEBUG output (ORANGE) */
        dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x6e00 | ('A' + glob_reqdrv);
        dbg_VGA[dbg_startoffset + dbg_xpos++] = 0x6e00 | ':';
      #endif
        if (glob_reqdrv != glob_ldrv) goto CHAINTOPREVHANDLER;
        if (r.h.al != AL_DISKSPACE) gen_fcbname_from_path();
        }
        break;
    }
  }
  if (glob_reqdrv != glob_ldrv) goto CHAINTOPREVHANDLER;

  /* copy interrupt registers into glob_intregs so the int handler can access them without using any stack */
  copybytes(&glob_intregs, &r, sizeof(union INTPACK));
  /* set stack to my custom memory */
  _asm {
    cli /* make sure to disable interrupts, so nobody gets in the way while I'm fiddling with the stack */
    mov glob_oldstack_seg, SS
    mov glob_oldstack_off, SP
    /* set SS to DS */
    mov ax, ds
    mov ss, ax
    /* set SP to the end of my DATASEGSZ (-2) */
    mov sp, DATASEGSZ
    dec sp
    dec sp
    sti
  }
  /* call the actual INT 2F processing function */
  process2f();
  /* switch stack back */
  _asm {
    cli
    mov SS, glob_oldstack_seg
    mov SP, glob_oldstack_off
    sti
  }
  /* copy all registers back so watcom will set them as required 'for real' */
  copybytes(&r, &glob_intregs, sizeof(union INTPACK));
  return;

  /* hand control to the previous INT 2F handler */
  CHAINTOPREVHANDLER:
  _mvchain_intr(MK_FP(glob_prev_2f_handler_seg, glob_prev_2f_handler_off));
}


/*********************** HERE ENDS THE RESIDENT PART ***********************/

#pragma code_seg("_TEXT", "CODE");

/* this function is obviously useless - but I do need it because it is a
 * 'low-water' mark for the end of my resident code */
void begtextend(void) {
}

/* registers a packet driver handle to use on subsequent calls */
static int pktdrv_accesstype(void) {
  static unsigned char etype[2] = {0xED, 0xF5}; /* my custom ethertype */
  unsigned char cflag = 0;

  _asm {
    mov ax, 201h        /* AH=subfunction access_type(), AL=if_class=1(eth) */
    mov bx, 0ffffh      /* if_type = 0xffff means 'all' */
    mov dl, 0           /* if_number: 0 (first interface) */
    /* I don't modify DS, it already points to the segment of etype */
    mov si, offset etype /* DS:SI should point to etype[] */
    mov cx, 2           /* typelen (ethertype is 16 bits) */
    /* ES:DI points to the receiving routine */
    push cs /* write segment of pktdrv_recv into es */
    pop es
    mov di, offset pktdrv_recv
    mov cflag, 1        /* pre-set the cflag variable to failure */
    /* int to variable vector is a mess, so I have fetched its vector myself
     * and pushf + cli + call far it now to simulate a regular int */
    pushf
    cli
    call dword ptr glob_pktdrv_pktcall
    /* get CF state - reset cflag if CF clear, and get pkthandle from AX */
    jc badluck   /* Jump if Carry */
    mov glob_pktdrv_pkthandle, ax /* Pkt handle should be in AX */
    mov cflag, 0
    badluck:
  }

  if (cflag != 0) return(-1);
  return(0);
}

/* get my own MAC addr. target MUST point to a space of at least 6 chars */
static void pktdrv_getaddr(unsigned char *dst) {
  _asm {
    mov ah, 6                       /* subfunction: get_addr() */
    mov bx, glob_pktdrv_pkthandle;  /* handle */
    push ds                         /* write segment of dst into es */
    pop es
    mov di, dst                     /* offset of dst (in small mem model dst IS an offset) */
    mov cx, 6                       /* expected length (ethernet = 6 bytes) */
    /* int to variable vector is a mess, so I have fetched its vector myself
     * and pushf + cli + call far it now to simulate a regular int */
    pushf
    cli
    call dword ptr glob_pktdrv_pktcall
  }
}


static int pktdrv_init(unsigned short pktintparam) {
  unsigned short far *intvect = (unsigned short far *)MK_FP(0, pktintparam * 4);
  unsigned short pktdrvfuncoffs = *intvect;
  unsigned short pktdrvfuncseg = *(intvect+1);
  unsigned short rseg = 0, roff = 0;
  char far *pktdrvfunc = (char far *)MK_FP(pktdrvfuncseg, pktdrvfuncoffs);
  int i;
  char sig[8];
  /* preload sig with "PKT DRVR" -- I could it just as well with
   * char sig[] = "PKT DRVR", but I want to avoid this landing in
   * my DATA segment so it doesn't pollute the TSR memory space. */
  sig[0] = 'P';
  sig[1] = 'K';
  sig[2] = 'T';
  sig[3] = ' ';
  sig[4] = 'D';
  sig[5] = 'R';
  sig[6] = 'V';
  sig[7] = 'R';

  pktdrvfunc += 3; /* skip three bytes of executable code */
  for (i = 0; i < 8; i++) if (sig[i] != pktdrvfunc[i]) return(-1);

  glob_pktdrv_pktint = pktintparam;

  /* fetch the vector of the pktdrv interrupt and save it for later */
  _asm {
    mov ah, 35h /* AH=GetVect */
    mov al, glob_pktdrv_pktint; /* AL=int number */
    push es /* save ES and BX (will be overwritten) */
    push bx
    int 21h
    mov rseg, es
    mov roff, bx
    pop bx
    pop es
  }
  glob_pktdrv_pktcall = MK_FP(rseg, roff);

  return(pktdrv_accesstype());
}


static void pktdrv_free(void) {
  _asm {
    mov ah, 3
    mov bx, glob_pktdrv_pkthandle
    /* int to variable vector is a mess, so I have fetched its vector myself
     * and pushf + cli + call far it now to simulate a regular int */
    pushf
    cli
    call dword ptr glob_pktdrv_pktcall
  }
  /* if (regs.x.cflag != 0) return(-1);
  return(0);*/
}

static struct sdastruct far *getsda(void) {
  /* DOS 3.0+ - GET ADDRESS OF SDA (Swappable Data Area)
   * AX = 5D06h
   *
   * CF set on error (AX=error code)
   * DS:SI -> sda pointer
   */
  unsigned short rds = 0, rsi = 0;
  _asm {
    mov ax, 5d06h
    push ds
    push si
    int 21h
    mov bx, ds
    mov cx, si
    pop si
    pop ds
    mov rds, bx
    mov rsi, cx
  }
  return(MK_FP(rds, rsi));
}

/* returns the CDS struct for drive. requires DOS 4+ */
static struct cdsstruct far *getcds(unsigned int drive) {
  /* static to preserve state: only do init once */
  static unsigned char far *dir;
  static int ok = -1;
  static unsigned char lastdrv;
  /* init of never inited yet */
  if (ok == -1) {
    /* DOS 3.x+ required - no CDS in earlier versions */
    ok = 1;
    /* offsets of CDS and lastdrv in the List of Lists depends on the DOS version:
     * DOS < 3   no CDS at all
     * DOS 3.0   lastdrv at 1Bh, CDS pointer at 17h
     * DOS 3.1+  lastdrv at 21h, CDS pointer at 16h */
    /* fetch lastdrv and CDS through a little bit of inline assembly */
    _asm {
      push si /* SI needs to be preserved */
      /* get the List of Lists into ES:BX */
      mov ah, 52h
      int 21h
      /* get the LASTDRIVE value */
      mov si, 21h /* 21h for DOS 3.1+, 1Bh on DOS 3.0 */
      mov ah, byte ptr es:[bx+si]
      mov lastdrv, ah
      /* get the CDS */
      mov si, 16h /* 16h for DOS 3.1+, 17h on DOS 3.0 */
      les bx, es:[bx+si]
      mov word ptr dir+2, es
      mov word ptr dir, bx
      /* restore the original SI value*/
      pop si
    }
    /* some OSes (at least OS/2) set the CDS pointer to FFFF:FFFF */
    if (dir == (unsigned char far *) -1l) ok = 0;
  } /* end of static initialization */
  if (ok == 0) return(NULL);
  if (drive > lastdrv) return(NULL);
  /* return the CDS array entry for drive - note that currdir_size depends on
   * DOS version: 0x51 on DOS 3.x, and 0x58 on DOS 4+ */
  return((struct cdsstruct __far *)((unsigned char __far *)dir + (drive * 0x58 /*currdir_size*/)));
}
/******* end of CDS-related stuff *******/

/* primitive BIOS-based message output used instead of printf() to limit
 * memory usage and binary size */
static void outmsg(char *s) {
  _asm {
    mov ah, 9h  /* DOS 1+ - WRITE STRING TO STANDARD OUTPUT */
    mov dx, s   /* small memory model: no need to set DS, 's' is an offset */
    int 21h
  }
}


/* returns 0 if ethdrive is not found in memory, non-zero otherwise
   NOTE: checking for self won't work if another TSR chained int 2F ! I should
         rather do some finger matching through an AX=2F00 call...
         On the other hand, packet driver access will be refused the second
         time anyway, so detecting self is a purely cosmetic issue. */
static int tsrpresent(void) {
  unsigned char far *ptr = (unsigned char far *)MK_FP(glob_prev_2f_handler_seg, glob_prev_2f_handler_off) + 23; /* the interrupt handler's signature appears at offset 23 (this might change at each source code modification) */

  /*{ unsigned short far *VGA = (unsigned short far *)(0xB8000000l);
  for (x = 0; x < 512; x++) VGA[240 + ((x >> 6) * 80) + (x & 63)] = 0x1f00 | ptr[x]; }*/

  /* look for the "MVethd" signature */
  if ((ptr[0] != 'M') || (ptr[1] != 'V') || (ptr[2] != 'e') || (ptr[3] != 't')
   || (ptr[4] != 'h') || (ptr[5] != 'd')) return(0);
  return(1);
}

/* zero out an object of l bytes */
static void zerobytes(void *obj, unsigned short l) {
  unsigned char *o = obj;
  while (l-- != 0) {
    *o = 0;
    o++;
  }
}

/* expects a hex string of exactly two chars "XX" and returns its value, or -1
 * if invalid */
static int hexpair2int(char *hx) {
  unsigned char h[2];
  unsigned short i;
  /* translate hx[] to numeric values and validate */
  for (i = 0; i < 2; i++) {
    if ((hx[i] >= 'A') && (hx[i] <= 'F')) {
      h[i] = hx[i] - ('A' - 10);
    } else if ((hx[i] >= 'a') && (hx[i] <= 'f')) {
      h[i] = hx[i] - ('a' - 10);
    } else if ((hx[i] >= '0') && (hx[i] <= '9')) {
      h[i] = hx[i] - '0';
    } else { /* invalid */
      return(-1);
    }
  }
  /* compute the end result and return it */
  i = h[0];
  i <<= 4;
  i |= h[1];
  return(i);
}

/* translates an ASCII MAC address into a 6-bytes binary string */
static int string2mac(unsigned char *d, char *mac) {
  int i, j, v;
  /* is it exactly 17 chars long? */
  for (i = 0; mac[i] != 0; i++);
  if (i != 17) return(-1);
  /* are nibble pairs separated by colons? */
  for (i = 2; i < 16; i += 3) if (mac[i] != ':') return(-1);
  /* translate each byte to its numeric value */
  j = 0;
  for (i = 0; i < 16; i += 3) {
    v = hexpair2int(mac + i);
    if (v < 0) return(-1);
    mac[j++] = v;
  }
  return(0);
}


#define ARGFL_QUIET 1
#define ARGFL_AUTO 2

/* a structure used to pass and decode arguments between main() and parseargv() */
struct argstruct {
  int argc;    /* original argc */
  char **argv; /* original argv */
  unsigned short pktint; /* custom packet driver interrupt */
  unsigned char flags; /* ARGFL_QUIET, ARGFL_AUTO */
};


/* parses (and applies) command-line arguments. returns 0 on success,
 * non-zero otherwise */
static int parseargv(struct argstruct *args) {
  /* Syntax: etherdfs SRVMAC rdrive:ldrive [options] */
  int i;
  /* I require at least 2 arguments */
  if (args->argc < 3) return(-1);
  /* read the srv mac address, unless it's "::" (auto) */
  if ((args->argv[1][0] == ':') && (args->argv[1][1] == ':') && (args->argv[1][2] == 0)) {
    args->flags |= ARGFL_AUTO;
  } else {
    if (string2mac(glob_rmac, args->argv[1]) != 0) return(-1);
  }

  /* is rdrive:ldrive option sane? */
  if ((args->argv[2][0] < 'A') || (args->argv[2][1] != '-') || (args->argv[2][2] < 'A') || (args->argv[2][3] != 0)) return(-2);
  glob_rdrv = DRIVETONUM(args->argv[2][0]);
  glob_ldrv = DRIVETONUM(args->argv[2][2]);
  /* iterate through options, if any */
  for (i = 3; i < args->argc; i++) {
    char opt;
    char *arg;
    if ((args->argv[i][0] != '/') || (args->argv[i][1] == 0)) return(-3);
    opt = args->argv[i][1];
    /* fetch option's argument, if any */
    if (args->argv[i][2] == 0) { /* single option */
      arg = NULL;
    } else if (args->argv[i][2] == '=') { /* trailing argument */
      arg = args->argv[i] + 3;
    } else {
      return(-3);
    }
    /* normalize the option char to lower case */
    if ((opt >= 'A') && (opt <= 'Z')) opt += ('a' - 'A');
    /* what is the option about? */
    switch (opt) {
      case 'q':
        if (arg != NULL) return(-4);
        args->flags |= ARGFL_QUIET;
        break;
      case 'p':
        if (arg == NULL) return(-4);
        /* I expect an exactly 2-characters string */
        if ((arg[0] == 0) || (arg[1] == 0) || (arg[2] != 0)) return(-1);
        if ((args->pktint = hexpair2int(arg)) < 1) return(-4);
        break;
      default: /* invalid parameter */
        return(-5);
    }
  }
  return(0);
}

/* translates an unsigned byte into a 2-characters string containing its hex
 * representation. s needs to be at least 3 bytes long. */
static void byte2hex(char *s, unsigned char b) {
  char h[16];
  unsigned short i;
  /* pre-compute h[] with a string 0..F -- I could do the same thing easily
   * with h[] = "0123456789ABCDEF", but then this would land inside the DATA
   * segment, while I want to keep it in stack to avoid polluting the TSR's
   * memory space */
  for (i = 0; i < 10; i++) h[i] = '0' + i;
  for (; i < 16; i++) h[i] = ('A' - 10) + i;
  /* */
  s[0] = h[b >> 4];
  s[1] = h[b & 15];
  s[2] = 0;
}

/* allocates sz bytes of memory and returns the segment to allocated memory or
 * 0 on error */
static unsigned short allocseg(unsigned short sz) {
  unsigned short volatile res = 0;
  /* sz should contains number of 16-byte paragraphs instead of bytes */
  sz += 15; /* make sure to allocate enough paragraphs */
  sz >>= 4;
  /* ask DOS for memory */
  _asm {
    mov ah, 48h     /* alloc memory (DOS 2+) */
    mov bx, sz      /* number of paragraphs to allocate */
    mov res, 0      /* pre-set res to failure (0) */
    int 21h         /* returns allocated segment in AX */
    /* check CF */
    jc failed
    mov res, ax     /* set res to actual result */
    failed:
  }
  return(res);
}

/* free segment previously allocated through allocseg() */
static void freeseg(unsigned short segm) {
  _asm {
    mov ah, 49h   /* free memory (DOS 2+) */
    mov es, segm  /* put segment to free into ES */
    int 21h
  }
}

/* patch the TSR routine and packet driver handler so they use my new DS.
 * return 0 on success, non-zero otherwise */
static int updatetsrds(void) {
  unsigned short newds;
  unsigned char far *ptr;
  unsigned short far *sptr;
  newds = 0;
  _asm {
    push ds
    pop newds
  }

  /* first patch the TSR routine */
  ptr = (unsigned char far *)inthandler + 23; /* the interrupt handler's signature appears at offset 23 (this might change at each source code modification and/or optimization settings) */
  sptr = (unsigned short far *)ptr;
  /* check for the routine's signature first */
  if ((ptr[0] != 'M') || (ptr[1] != 'V') || (ptr[2] != 'e') || (ptr[3] != 't')) return(-1);
  sptr[4] = newds;
  /* now patch the pktdrv_recv() routine */
  ptr = (unsigned char far *)pktdrv_recv + 3;
  sptr = (unsigned short far *)ptr;
  /*{
    int x;
    unsigned short far *VGA = (unsigned short far *)(0xB8000000l);
    for (x = 0; x < 128; x++) VGA[80*12 + ((x >> 6) * 80) + (x & 63)] = 0x1f00 | ptr[x];
  }*/
  /* check for the routine's signature first */
  if ((ptr[0] != 'p') || (ptr[1] != 'k') || (ptr[2] != 't') || (ptr[3] != 'r')) return(-1);
  sptr[4] = newds;
  /*{
    int x;
    unsigned short far *VGA = (unsigned short far *)(0xB8000000l);
    for (x = 0; x < 128; x++) VGA[80*20 + ((x >> 6) * 80) + (x & 63)] = 0x1f00 | ptr[x];
  }*/
  return(0);
}


int main(int argc, char **argv) {
  struct argstruct args;
  struct cdsstruct far *cds;
  unsigned char dosver = 0;
  unsigned short volatile newdataseg; /* 'volatile' just in case the compiler would try to optimize it out, since I set it through in-line assembly */

  /* parse command-line arguments */
  zerobytes(&args, sizeof(args));
  args.argc = argc;
  args.argv = argv;
  if (parseargv(&args) != 0) {
    #include "msg/help.c"
    return(1);
  }

  /* check DOS version - I require DOS 5.0+ */
  _asm {
    mov ax, 3306h
    int 21h
    mov dosver, bl
    inc al /* if AL was 0xFF ("unsupported function"), it is 0 now */
    jnz done
    mov dosver, 0 /* if AL is 0 (hence was 0xFF), set dosver to 0 */
    done:
  }
  if (dosver < 5) {
    #include "msg\\unsupdos.c"
    return(1);
  }

  /* remember current int 2f handler, we might over-write it soon (also I
   * use it to see if I'm already loaded) */
  _asm {
    mov ax, 352fh; /* AH=GetVect AL=2F */
    push es /* save ES and BX (will be overwritten) */
    push bx
    int 21h
    mov glob_prev_2f_handler_seg, es
    mov glob_prev_2f_handler_off, bx
    pop bx
    pop es
  }

  /* is the TSR installed already? */
  if (tsrpresent() != 0) {
    #include "msg\\alrload.c"
    return(1);
  }

  /* enable the new drive */
  cds = getcds(glob_ldrv);
  if (cds == NULL) {
    #include "msg\\mapfail.c"
    return(1);
  }

  /* if the requested drive is already active, fail */
  if (cds->flags != 0) {
    #include "msg\\drvactiv.c"
    return(1);
  }

  /* allocate a new segment for all my internal needs, and use it right away
   * as DS */
  newdataseg = allocseg(DATASEGSZ);
  if (newdataseg == 0) {
    #include "msg\\memfail.c"
    return(1);
  }

  /* copy current DS into the new segment and switch to new DS/SS */
  _asm {
    /* save registers on the stack */
    push es
    push cx
    push si
    push di
    pushf
    /* copy the memory block */
    mov cx, DATASEGSZ  /* copy cx bytes */
    xor si, si         /* si = 0*/
    xor di, di         /* di = 0 */
    cld                /* clear direction flag (increment si/di) */
    mov es, newdataseg /* load es with newdataseg */
    rep movsb          /* execute copy DS:SI -> ES:DI */
    /* restore registers (but NOT es, instead save it into AX for now) */
    popf
    pop di
    pop si
    pop cx
    pop ax
    /* switch to the new DS _AND_ SS now */
    push es
    push es
    pop ds
    pop ss
    /* restore ES */
    push ax
    pop es
  }

  /* patch the TSR and pktdrv_recv() so they use my new DS */
  if (updatetsrds() != 0) {
    #include "msg\\relfail.c"
    freeseg(newdataseg);
    return(1);
  }

  /* remember the SDA address (will be useful later) */
  glob_sdaptr = getsda();

  /* init the packet driver interface */
  glob_pktdrv_pktint = 0;
  if (args.pktint == 0) { /* detect first packet driver within int 60h..80h */
    int i;
    for (i = 0x60; i <= 0x80; i++) {
      if (pktdrv_init(i) == 0) break;
    }
  } else { /* use the pktdrvr interrupt passed through command line */
    pktdrv_init(args.pktint);
  }
  /* has it succeeded? */
  if (glob_pktdrv_pktint == 0) {
    #include "msg\\pktdfail.c"
    freeseg(newdataseg);
    return(1);
  }
  pktdrv_getaddr(glob_lmac);

  /* should I auto-discover the server? */
  if ((args.flags & ARGFL_AUTO) != 0) {
    unsigned short i, *ax;
    unsigned char *answer;
    /* set (temporarily) glob_rmac to broadcast */
    for (i = 0; i < 6; i++) glob_rmac[i] = 0xff;
    /* send a discovery frame that will update glob_rmac */
    if (sendquery(AL_DISKSPACE, glob_ldrv, 0, &answer, &ax, 1) != 6) {
      #include "msg\\nosrvfnd.c"
      pktdrv_free(); /* free the pkt drv and quit */
      freeseg(newdataseg);
      return(1);
    }
  }

  /* set the drive as being a 'network' disk */
  cds->flags = CDSFLAG_NET;

  if ((args.flags & ARGFL_QUIET) == 0) {
    char buff[20];
    int i;
    #include "msg\\instlled.c"
    for (i = 0; i < 6; i++) {
      byte2hex(buff + i + i + i, glob_lmac[i]);
    }
    for (i = 2; i < 16; i += 3) buff[i] = ':';
    buff[17] = '$';
    outmsg(buff);
    outmsg(", pktdrvr at INT $");
    byte2hex(buff, glob_pktdrv_pktint);
    buff[2] = '$';
    outmsg(buff);
    outmsg(")\r\n  $");
    buff[0] = 'A' + glob_ldrv;
    buff[1] = '$';
    outmsg(buff);
    outmsg(": -> [$");
    buff[0] = 'A' + glob_rdrv;
    buff[1] = '$';
    outmsg(buff);
    outmsg(":] on $");
    for (i = 0; i < 6; i++) {
      byte2hex(buff + i + i + i, glob_rmac[i]);
    }
    for (i = 2; i < 16; i += 3) buff[i] = ':';
    buff[17] = '\r';
    buff[18] = '\n';
    buff[19] = '$';
    outmsg(buff);
  }

  /* get the segment of the PSP (might come handy later) */
  _asm {
    mov ah, 62h          /* get current PSP address */
    int 21h              /* returns the segment of PSP in BX */
    mov glob_pspseg, bx  /* copy PSP segment to glob_pspseg */
  }

  /* free the environment (env segment is at offset 2C of the PSP) */
  _asm {
    mov es, glob_pspseg /* load ES with PSP's segment */
    mov es, es:[2Ch]    /* get segment of the env block */
    mov ah, 49h         /* free memory (DOS 2+) */
    int 21h
  }

  /* set up the TSR (INT 2F catching) */
  _asm {
    cli
    mov ax, 252fh /* AH=set interrupt vector  AL=2F */
    push ds /* preserve DS and DX */
    push dx
    push cs /* set DS to current CS, that is provide the */
    pop ds  /* int handler's segment */
    mov dx, offset inthandler /* int handler's offset */
    int 21h
    pop dx /* restore DS and DX to previous values */
    pop ds
    sti
  }

  /* Turn self into a TSR and free memory I won't need any more. That is, I
   * free all the libc startup code and my init functions by passing the
   * number of paragraphs to keep resident to INT 21h, AH=31h. How to compute
   * the number of paragraphs? Simple: look at the memory map and note down
   * the size of the BEGTEXT segment (that's where I store all TSR routines).
   * then: (sizeof(BEGTEXT) + sizeof(PSP) + 15) / 16
   * PSP is 256 bytes of course. And +15 is needed to avoid truncating the
   * last (partially used) paragraph. */
  _asm {
    mov ax, 3100h  /* AH=31 'terminate+stay resident', AL=0 exit code */
    mov dx, offset begtextend /* DX = offset of resident code end     */
    add dx, 256    /* add size of PSP (256 bytes)                     */
    add dx, 15     /* add 15 to avoid truncating last paragraph       */
    shr dx, 1      /* convert bytes to number of 16-bytes paragraphs  */
    shr dx, 1      /* the 8086/8088 CPU supports only a 1-bit version */
    shr dx, 1      /* of SHR, so I have to repeat it as many times as */
    shr dx, 1      /* many bits I need to shift.                      */
    int 21h
  }

  return(0); /* never reached, but compiler complains if not present */
}
