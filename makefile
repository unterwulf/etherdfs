#
# Makefile for etherdfs, requires Open Watcom
# Copyright (C) 2017 Mateusz Viste
#
# http://etherdfs.sourceforge.net
#

all: etherdfs.exe

etherdfs.exe: etherdfs.c dosstruc.h globals.h
	wcl -y -0 -s -d0 -lr -ms -we -wx -k2048 -fm=etherdfs.map -os etherdfs.c

# -y      ignore the WCL env. variable, if any
# -0      generate code for 8086
# -s      disable stack overflow checks
# -d0     don't include debugging information
# -lr     compile to a DOS real-mode application
# -ms     small memory model
# -we     treat all warnings as errors
# -wx     set warning level to max
# -k2048  set stack size to 2048 bytes (for the non-resident part)
# -fm=    generate a map file
# -os     optimize for size

clean: .symbolic
	if exist etherdfs.exe del etherdfs.exe

pkg: .symbolic etherdfs.exe
	if exist etherdfs.zip del etherdfs.zip
	zip -9 -k etherdfs.zip etherdfs.exe etherdfs.txt
	if exist ethersrc.zip del ethersrc.zip
	zip -9 -k ethersrc.zip *.h *.c *.txt makefile
