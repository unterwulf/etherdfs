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

/* required size (in bytes) of the data segment - this must be bigh enough as
 * to accomodate all "DATA" segments AND the stack, which will be located at
 * the very end of the data segment. packet drivers tend to require a stack
 * of several hundreds bytes at least - 1K should be safe... It is important
 * that DATASEGSZ can contain a stack of AT LEAST the size of the stack used
 * by the transient code, since the transient part of the program will switch
 * to it and expects the stack to not become corrupted in the process */
#define DATASEGSZ 3500

/* a few globals useful only for debug messages */
#if DEBUGLEVEL > 0
static unsigned short dbg_xpos = 0;
static unsigned short far *dbg_VGA = (unsigned short far *)(0xB8000000l);
static unsigned char dbg_hexc[16] = "0123456789ABCDEF";
#define dbg_startoffset 80*16
#endif

static unsigned char glob_ldrv;    /* local drive letter (0=A:, 1=B:, etc) */
static unsigned char glob_rdrv;    /* remote drive (0=A:, 1=B:, etc) */

/* global variables related to packet driver management and handling frames */
static unsigned char glob_pktdrv_recvbuff[FRAMESIZE];
static signed short volatile glob_pktdrv_recvbufflen; /* length of the frame in buffer, 0 means "free", and neg value means "awaiting" */
static unsigned short glob_pktdrv_pkthandle;  /* handler returned by the packet driver */
static unsigned char glob_pktdrv_sndbuff[FRAMESIZE]; /* this not only is my send-frame buffer, but I also use it to store permanently lmac, rmac, ethertype and PROTOVER at proper places */
static unsigned char glob_pktdrv_pktint;      /* software interrupt of the packet driver */
static void (interrupt far *glob_pktdrv_pktcall)(); /* vector address of the pktdrv interrupt */

/* a few definitions for data that points to my sending buffer */
#define GLOB_LMAC (glob_pktdrv_sndbuff + 6) /* local MAC address */
#define GLOB_RMAC (glob_pktdrv_sndbuff)     /* remote MAC address */
static unsigned short *glob_ethertype = ((unsigned short *)glob_pktdrv_sndbuff)+6; /* ethertype field */

static unsigned char glob_reqdrv;  /* the requested drive, set by the INT 2F *
                                    * handler and read by process2f()        */

static unsigned short glob_reqstkword; /* WORD saved from the stack (used by SETATTR) */
static struct sdastruct far *glob_sdaptr; /* pointer to DOS SDA (set by main() at *
                                           * startup, used later by process2f()   */

static unsigned short glob_pspseg; /* segment of the program's PSP block */

static unsigned short glob_prev_2f_handler_seg; /* seg:off of the previous   */
static unsigned short glob_prev_2f_handler_off; /* 2F handler (so I can call */
                                                /* it for all queries that   */
                                                /* do not relate to my drive */

/* seg:off addresses of the old (DOS) stack */
static unsigned short glob_oldstack_seg;
static unsigned short glob_oldstack_off;

/* an INTPACK structure used to store registers as set when INT2F is called */
static union INTPACK glob_intregs;


#endif
