CC = aarch64-linux-gnu-gcc
CFLAGS = -static -O2 -march=armv8.4-a+nosve
BINDIR = ../bin/arm/linux

TESTS = test_dit_enable test_dit_disable test_dit_read test_dit_toggle test_dit_compsimp test_dit_loop test_dit_loop_nop

all: $(TESTS)

%: %.c
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $<

clean:
	rm -f $(BINDIR)/test_dit_*

.PHONY: all clean
