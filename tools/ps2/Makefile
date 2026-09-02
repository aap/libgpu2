# gsrun: GS-dump replayer for the DTL-T10000 (SCE SDK 3.0, ee-gcc).
#     make            build gsrun.elf
#     make run DIR=host0:../../out/<name>/hw     (via dsedb)
SHELL   = /bin/sh
TOP     = /usr/local/sce/ee
LIBDIR  = $(TOP)/lib
INCDIR  = $(TOP)/include
BIN     = $(TOP)/gcc/bin

TARGET  = gsrun
OBJS    = crt0.o gsrun.o
LCFILE  = $(LIBDIR)/app.cmd
LIBS    = $(LIBDIR)/libgraph.a $(LIBDIR)/libdma.a $(LIBDIR)/libpkt.a

CC      = $(BIN)/ee-gcc
CFLAGS  = -O2 -Wall -fno-common -fno-strict-aliasing
LDFLAGS = -Wl,-Map,$(TARGET).map -mno-crt0 -L$(LIBDIR) -lm

all: $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) -o $@ -T $(LCFILE) $(OBJS) $(LIBS) $(LDFLAGS)

crt0.o: $(LIBDIR)/crt0.s
	$(CC) -c -xassembler-with-cpp -o $@ $<

gsrun.o: gsrun.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

run: $(TARGET).elf
	dsedb -r run $(TARGET).elf $(DIR)

clean:
	rm -f *.o *.map *.elf
