/*
 * This file is part of the etherdfs project.
 * http://etherdfs.sourceforge.net
 *
 * Copyright (C) 2017 Mateusz Viste
 *
 * Contains all global variables used by etherdfs.
 */

#ifndef GLOBALS_SENTINEL
#define GLOBALS_SENTINEL

/* a few globals useful only for debug messages */
#if DEBUGLEVEL > 0
static unsigned short dbg_xpos = 0;
static unsigned short far *dbg_VGA = (unsigned short far *)(0xB8000000l);
static unsigned char dbg_hexc[16] = "0123456789ABCDEF";
#define dbg_startoffset 80*16
#endif

static unsigned char glob_ldrv;    /* local drive letter (0=A:, 1=B:, etc) */
static unsigned char glob_rdrv;    /* remote drive (0=A:, 1=B:, etc) */

static unsigned char glob_lmac[6]; /* local MAC address */
static unsigned char glob_rmac[6]; /* remote MAC address */

static unsigned char glob_reqdrv;  /* the requested drive, set by the INT 2F *
                               * handler and read by process2f()        */

static struct sdastruct far *glob_sdaptr; /* pointer to DOS SDA (set by main() at *
                                           * startup, used later by process2f()   */

static unsigned short glob_pspseg; /* segment of the program's PSP block */

static void (__interrupt __far *glob_prev_2f_handler)(); /* previous INT 2F handler
                                                          * (so I can call it for
                                                          * all queries that do not
                                                          * relate to my drive) */

/* address of the internal stack that will be used by the TSR, as well as the
 * stack memory block itself */
static unsigned char glob_tsrstack[1024];
static unsigned short glob_tsrstack_seg;
static unsigned short glob_tsrstack_off;
/* seg:off addresses of the old (DOS) stack */
static unsigned short glob_oldstack_seg;
static unsigned short glob_oldstack_off;

/* an INTPACK structure used to store registers as set when INT2F is called */
static union INTPACK glob_intregs;

/* global variables related to packet driver management and handling frames */
static unsigned char glob_pktdrv_recvbuff[FRAMESIZE];
static signed short volatile glob_pktdrv_recvbufflen; /* length of the frame in buffer, 0 means "free", and neg value means "awaiting" */
static unsigned short glob_pktdrv_pkthandle;  /* handler returned by the packet driver */
static unsigned char glob_pktdrv_sndbuff[FRAMESIZE];
static unsigned char glob_pktdrv_pktint;      /* software interrupt of the packet driver */
static void (interrupt far *glob_pktdrv_pktcall)(); /* vector address of the pktdrv interrupt */


#endif
