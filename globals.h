/*
 * This file is part of the etherdfs project.
 * http://etherdfs.sourceforge.net
 *
 * Copyright (C) 2017 Mateusz Viste
 *
 * Contains all global variables used by etherdfs that should remain
 * available in TSR part.
 */

#ifndef GLOBALS_SENTINEL
#define GLOBALS_SENTINEL

/* set DEBUGLEVEL to 0, 1 or 2 to turn on debug mode with desired verbosity */
#define DEBUGLEVEL 0

/* define the maximum size of a frame, as sent or received by etherdfs.
 * example: value 1084 accomodates payloads up to 1024 bytes +all headers */
#define FRAMESIZE 1090

/* required size (in bytes) of the TSR stack, which will be located at
 * the very end of the data segment. packet drivers tend to require a stack
 * of several hundreds bytes at least - 1K should be safe... */
#define STKSZ 1024

/* a few globals useful only for debug messages */
#if DEBUGLEVEL > 0
extern unsigned short dbg_xpos;
extern unsigned short far *dbg_VGA;
extern unsigned char dbg_hexc[16];
#define dbg_startoffset 80*16
#endif

/* whenever the tsrshareddata structure changes, offsets below MUST be
 * adjusted (these are required by assembly routines) */
#define GLOB_DATOFF_PREV2FHANDLERSEG 0
#define GLOB_DATOFF_PREV2FHANDLEROFF 2
#define GLOB_DATOFF_PSPSEG 4
#define GLOB_DATOFF_PKTHANDLE 6
#define GLOB_DATOFF_PKTINT 8
struct tsrshareddata {
/*offs*/
/*  0 */ unsigned short prev_2f_handler_seg; /* seg:off of the previous 2F handler */
/*  2 */ unsigned short prev_2f_handler_off; /* (so I can call it for all queries  */
                                            /* that do not relate to my drive     */
/*  4 */ unsigned short pspseg;    /* segment of the program's PSP block */
/*  6 */ unsigned short pkthandle; /* handler returned by the packet driver */
/*  8 */ unsigned char pktint;     /* software interrupt of the packet driver */

         unsigned char ldrv[26]; /* local to remote drives mappings (0=A:, 1=B, etc */
};

extern struct tsrshareddata glob_data;

/* global variables related to packet driver management and handling frames */
extern unsigned char glob_pktdrv_recvbuff[FRAMESIZE];
extern signed short volatile glob_pktdrv_recvbufflen; /* length of the frame in buffer, 0 means "free", and neg value means "awaiting" */
extern unsigned char glob_pktdrv_sndbuff[FRAMESIZE]; /* this not only is my send-frame buffer, but I also use it to store permanently lmac, rmac, ethertype and PROTOVER at proper places */
extern unsigned long glob_pktdrv_pktcall;     /* vector address of the pktdrv interrupt */

/* a few definitions for data that points to my sending buffer */
#define GLOB_LMAC (glob_pktdrv_sndbuff + 6) /* local MAC address */
#define GLOB_RMAC (glob_pktdrv_sndbuff)     /* remote MAC address */

extern unsigned char glob_reqdrv;  /* the requested drive, set by the INT 2F *
                                    * handler and read by process2f()        */

extern unsigned short glob_reqstkword; /* WORD saved from the stack (used by SETATTR) */
extern struct sdastruct far *glob_sdaptr; /* pointer to DOS SDA (set by main() at *
                                           * startup, used later by process2f()   */

/* the INT 2F "multiplex id" registerd by EtherDFS */
extern unsigned char glob_multiplexid;

/* an INTPACK structure used to store registers as set when INT2F is called */
extern union INTPACK glob_intregs;

/* End of TSR text/data part. It's a label, not a real pointer. Only its
 * own address matters, its value should never be used. */
extern const void * const tsr_end_label;

/* Top of the TSR stack. It's a real pointer. */
extern void *tsr_stk_top;

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
extern unsigned char supportedfunctions[0x2F];

#endif
