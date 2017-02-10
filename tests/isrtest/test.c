/*
 * Test program: sending out a frame from within INT2F
 * Copyright (C) 2016 Mateusz Viste
 */

#include <dos.h>
#include <i86.h>
#include <stdlib.h> /* NULL */

/* make sure structs are packed tightly (required since that's how DOS packs its CDS) */
#pragma pack(1)

/* a few globals useful only for debug messages */
static unsigned char dbgxpos = 0;
static unsigned short far *VGA = (unsigned short far *)(0xB8000000l);

static unsigned char lmac[6];
static unsigned char rmac[6];
static void (__interrupt __far *prev_2f_handler)();

/* a global structure with few things related to packet driver management */
struct pktdrv_t {
  unsigned short pkthandle;  /* handler returned by the packet driver */
  unsigned short pktint;     /* software interrupt of the packet driver */
  signed short inbufflen;    /* length of the frame in buffer, 0 means "free" */
  unsigned char far *buff;
  unsigned char buffsize;    /* available max space in buffer (0 means no buffer) */
} pktdrv;


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


/* this function is called two times by the packet driver. One time for
 * telling that a packet is incoming, and how big it is, so the application
 * can prepare a buffer for it and hand it back to the packet driver. the
 * second call is just let know that the frame has been copied into the
 * buffer.
 * even though this is properly declared as an interrupt routine, the epilog
 * have to be hand-tuned (this has been lurked from mTCP). */
void far interrupt pktdrv_recv(union INTPACK r) {
  /* DEBUG("pktdrv_recv() called with AX=%u / CX=%u\n", r.w.ax, r.w.cx); */
  if (r.w.ax == 0) { /* first call: the packet driver needs a buffer */
    /* DEBUG("pktdrv requests %d bytes for incoming frame\n", r.w.cx); */
    if ((r.w.cx > pktdrv.buffsize) || (pktdrv.inbufflen > 0)) { /* no room or packet too big */
      /* DEBUG("no room or packet too big\n"); */
      r.w.es = 0;
      r.w.di = 0;
    } else { /* we can handle it. give out the first buffer on the free stack */
      /* DEBUG("buffer assigned\n"); */
      pktdrv.inbufflen = r.w.cx;
      pktdrv.inbufflen = 0 - pktdrv.inbufflen; /* switch value to negative until we receive actual data */
      r.w.es = FP_SEG(pktdrv.buff);
      r.w.di = FP_OFF(pktdrv.buff);
    }
  } else {
    /* DEBUG("pktdrv_recv() second call\n"); */
    /* this is the second pktdrv call - I am supposed to find a new packet
     * inside inbuffer. I switch inbufferlen back to a positive value so the
     * TCP/IP stack can see there's something available there. */
    pktdrv.inbufflen = 0 - pktdrv.inbufflen;
    /* DEBUG("a frame of %d bytes is available in buffer now\n", inbufferlen); */
  }

  /* custom epilog code (copied from mTCP). Rationale: "Some packet drivers
   * can handle the normal compiler generated epilog, but the Xircom PE3-10BT
   * drivers definitely can not." */
  _asm {
    pop ax
    pop ax
    pop es
    pop ds
    pop di
    pop si
    pop bp
    pop bx
    pop bx
    pop dx
    pop cx
    pop ax
    retf
  };
}

/* registers a packet driver handle to use on subsequent calls */
static int pktdrv_accesstype(void) {
  union REGS regs;
  struct SREGS segregs;
  static unsigned char etype[2] = {0xED, 0xF5}; /* my custom ethertype */

  regs.h.ah = 2;        /* subfunction: access_type()              */
  regs.h.al = 1;        /* if_class   : 1 (DIX Ethernet)           */
  regs.x.bx = 0xFFFFu;  /* if_type    : 0xFFFF for all             */
  regs.h.dl = 0;        /* if_number  : 0 (first interface)        */
  /* type (for ethernet, it would be a 16-bit ethertype) */
  segregs.ds = FP_SEG(etype);
  regs.x.si = FP_OFF(etype);
  regs.x.cx = 2;        /* typelen */
  segregs.es = FP_SEG(pktdrv_recv);
  regs.x.di = FP_OFF(pktdrv_recv);

  int86x(pktdrv.pktint, &regs, &regs, &segregs);

  if (regs.x.cflag != 0) return(-1);
  pktdrv.pkthandle = regs.x.ax;
  return(0);
}

/* get my own MAC addr. target MUST point to a space of at least 6 chars */
static void pktdrv_getaddr(unsigned char *dst) {
  union REGS regs;
  struct SREGS segregs;
  regs.h.ah = 6;                 /* subfunction: get_addr() */
  regs.x.bx = pktdrv.pkthandle;  /* handle */
  segregs.es = FP_SEG(dst);      /* buffer's segment */
  regs.x.di = FP_OFF(dst);       /* buffer's offset */
  regs.x.cx = 6;                 /* expected length (ethernet = 6 bytes) */
  int86x(pktdrv.pktint, &regs, &regs, &segregs);
}


static int pktdrv_init(unsigned short pktintparam) {
  unsigned short far *intvect = (unsigned short far *)MK_FP(0, pktintparam * 4);
  unsigned short pktdrvfuncoffs = *intvect;
  unsigned short pktdrvfuncseg = *(intvect+1);
  char far *pktdrvfunc = (char far *)MK_FP(pktdrvfuncseg, pktdrvfuncoffs);
  int i;
  char sig[8] = "PKT DRVR";

  pktdrvfunc += 3; /* skip three bytes of executable code */
  for (i = 0; i < 8; i++) if (sig[i] != pktdrvfunc[i]) return(-1);

  pktdrv.pktint = pktintparam;
  return(pktdrv_accesstype());
}

static unsigned char framebuf[1000];
static unsigned short framebuflen;
static unsigned short newstack_seg;
static unsigned short newstack_off;
/*static unsigned short oldstack_seg;
static unsigned short oldstack_off;*/

static unsigned short pktdrv_send(void) {
/*  _asm {
    mov oldstack_seg, SS
    mov oldstack_off, SP
    mov SS, newstack_seg
    mov SP, newstack_off
  }
  {*/
    union REGS regs;
    struct SREGS segregs;
    regs.x.ax = 0x0400;
    regs.h.dh = 0;
    regs.x.cx = framebuflen;
    segregs.ds = FP_SEG(framebuf);
    regs.x.si = FP_OFF(framebuf);
    int86x(pktdrv.pktint, &regs, &regs, &segregs);
  /*}
  _asm {
    mov SS, oldstack_seg
    mov SP, oldstack_off
  }*/

  return(framebuflen);
}


/* sends queries out */
static unsigned short sendquery(unsigned char query, unsigned char far *buff, unsigned short bufflen) {
  if (bufflen > (sizeof(framebuf) - 60)) bufflen = (sizeof(framebuf) - 60);
  copybytes((unsigned char far *)framebuf, (unsigned char far *)rmac, 6);
  copybytes((unsigned char far *)framebuf + 6, (unsigned char far *)lmac, 6);
  framebuf[12] = 0xED;
  framebuf[13] = 0xF5;
  framebuf[59] = query;
  copybytes((unsigned char far *)framebuf + 60, (unsigned char far *)buff, bufflen);

  framebuflen = bufflen + 60;
  pktdrv_send();
  return(bufflen + 60);
}


/* this function is hooked on INT 2Fh */
void __interrupt __far inthandler(union INTPACK r) {
  unsigned char buffarea[32]; /* small buffer for small queries */
  unsigned char far *buff = buffarea;

  /* DEBUG output (RED) */
  {
    static unsigned char hexc[16] = "0123456789ABCDEF";
    if (dbgxpos > 60) dbgxpos = 0;
    VGA[80 * 22 + dbgxpos++] = 0x4e00 | ' ';
    VGA[80 * 22 + dbgxpos++] = 0x4e00 | (hexc[(r.h.al >> 4) & 0xf]);
    VGA[80 * 22 + dbgxpos++] = 0x4e00 | (hexc[r.h.al & 0xf]);
    VGA[80 * 22 + dbgxpos++] = 0x4e00 | ' ';
  }

  sendquery(0, buff, 0);
  goto CHAINTOPREVHANDLER;
  return;

  /* hand control to the previous INT 2F handler */
  CHAINTOPREVHANDLER:
  _chain_intr(prev_2f_handler);
}

/* here ends the non-transient part */

/* primitive BIOS-based message output used instead of printf() to limit
 * memory usage and binary size */
static void outmsg(char *s) {
  union REGS r;
  r.h.ah = 0x9; /* DOS 1+ - WRITE STRING TO STANDARD OUTPUT */
  r.x.dx = FP_OFF(s); /* I don't set DS (small memory model) */
  int86(0x21, &r, &r);
}

int main(int argc, char **argv) {

  copybytes(rmac, "\xff\xff\xff\xff\xff\xff", 6);

  /* remember current int 2f handler, we might over-write it soon */
  prev_2f_handler = _dos_getvect(0x2f);

  /* init the packet driver interface FIXME: interrupt should be configurable */
  if (pktdrv_init(0x60) != 0) {
    outmsg("Packet driver initialization failed.\n$");
    return(1);
  }
  pktdrv_getaddr(lmac);

  /* DEBUG FIXME TODO this is only a test (sending works) */
  /* sendquery(0, NULL, 0); */

  {
    unsigned char *stack;
    stack = malloc(4096);
    newstack_seg = FP_SEG(stack);
    newstack_off = FP_OFF(stack);
  }

  /* set up the TSR (INT catching) */
  _dos_setvect(0x2f, inthandler);

  outmsg("installed\n$");

  _dos_keep(0, 1024);
  return(0);
}
