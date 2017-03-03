/*
 * This file is part of the etherdfs project.
 * http://etherdfs.sourceforge.net
 *
 * Copyright (C) 2017 Mateusz Viste
 *
 * Contains all global variables used by etherdfs that should remain
 * available in TSR part.
 */
#include <i86.h>     /* union INTPACK */
#include "globals.h"

#pragma data_seg("TSRDATA", "TSR")

/* a few globals useful only for debug messages */
#if DEBUGLEVEL > 0
unsigned short dbg_xpos = 0;
unsigned short far *dbg_VGA = (unsigned short far *)(0xB8000000l);
unsigned char dbg_hexc[16] = "0123456789ABCDEF";
#endif

struct tsrshareddata glob_data;

/* global variables related to packet driver management and handling frames */
unsigned char glob_pktdrv_recvbuff[FRAMESIZE];
signed short volatile glob_pktdrv_recvbufflen; /* length of the frame in buffer, 0 means "free", and neg value means "awaiting" */
unsigned char glob_pktdrv_sndbuff[FRAMESIZE]; /* this not only is my send-frame buffer, but I also use it to store permanently lmac, rmac, ethertype and PROTOVER at proper places */
unsigned long glob_pktdrv_pktcall;     /* vector address of the pktdrv interrupt */

unsigned char glob_reqdrv;  /* the requested drive, set by the INT 2F *
                             * handler and read by process2f()        */

unsigned short glob_reqstkword; /* WORD saved from the stack (used by SETATTR) */
struct sdastruct far *glob_sdaptr; /* pointer to DOS SDA (set by main() at *
                                    * startup, used later by process2f()   */

/* the INT 2F "multiplex id" registerd by EtherDFS */
unsigned char glob_multiplexid;

/* an INTPACK structure used to store registers as set when INT2F is called */
union INTPACK glob_intregs;

/* Top of the TSR stack */
void *tsr_stk_top;

/* this table makes it easy to figure out if I want a subfunction or not */
unsigned char supportedfunctions[0x2F] = {
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
