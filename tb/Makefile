# Makefile for DSC testbench
# Links against DSC library sources (without the CLI main)

SRCDIR = ../vesa_cmod/source

CC      = gcc
CFLAGS  = -std=gnu99 -O3 -Wall -D_FILE_OFFSET_BITS=64
INCPATH = -I$(SRCDIR)
LDFLAGS = -lm

# DSC library object files (excludes codec_main.c and main.cpp)
DSC_OBJS = \
	dsc_codec.o \
	dsc_utils.o \
	utl.o \
	fifo.o \
	multiplex.o \
	logging.o

# Testbench objects
TB_OBJS = dsc_tb.o

.PHONY: all clean

all: dsc_tb

# --- testbench executable ---
dsc_tb: $(TB_OBJS) $(DSC_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# --- testbench source ---
dsc_tb.o: dsc_tb.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

# --- library objects (compiled from SRCDIR) ---
dsc_codec.o: $(SRCDIR)/dsc_codec.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

dsc_utils.o: $(SRCDIR)/dsc_utils.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

utl.o: $(SRCDIR)/utl.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

fifo.o: $(SRCDIR)/fifo.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

multiplex.o: $(SRCDIR)/multiplex.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

logging.o: $(SRCDIR)/logging.c
	$(CC) $(CFLAGS) $(INCPATH) -c $< -o $@

# --- cleanup ---
clean:
	rm -f *.o dsc_tb
