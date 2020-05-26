#!/bin/sh

CC=gcc
LD=gcc

PROGS = mkstamfs stamfs2txt showdir
CFLAGS = -Wall -I../stamfs-standalone -D_GNU_SOURCE
LDFLAGS =

all: $(PROGS)

mkstamfs: mkstamfs.o
	$(LD) -o $@ $(LDFLAGS) $<

stamfs2txt: stamfs2txt.o
	$(LD) -o $@ $(LDFLAGS) $<

showdir: showdir.o
	$(LD) -o $@ $(LDFLAGS) $<

clean:
	/bin/rm -f $(PROGS) *.o core core.*
