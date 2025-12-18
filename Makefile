# Portable Makefile for 2.11BSD PDP-11 and macOS/Linux

CC = cc

PROG = bsdbasic

SRC = \
	src/main.c \
	src/basic.c \
	src/host_sleep.c

OBJ = src/main.o src/basic.o src/host_sleep.o

CPPFLAGS = -Iinclude
LDLIBS = -lm

# Keep code K&R-friendly: declarations at top of scope, avoid C99.
CFLAGS_HOST = -O2 -Wall -Wextra

# PDP-11 notes:
# - 2.11BSD often reports: `uname -m` => pdp11
# - 2.11BSD cc supports split I/D with `-i` (comment out if your toolchain differs).
CFLAGS_PDP11 = -O -DPDP11 -i

all:
	@m="`uname -m 2>/dev/null`"; \
	if [ "$$m" = "pdp11" ]; then \
		$(MAKE) realall CFLAGS="$(CFLAGS_PDP11)"; \
	else \
		$(MAKE) realall CFLAGS="$(CFLAGS_HOST)"; \
	fi

realall: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJ) $(LDLIBS)

src/main.o: src/main.c include/basic.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/main.c -o src/main.o

src/basic.o: src/basic.c src/basic_prelude.inc src/basic_core.inc src/basic_stmt.inc src/basic_vm.inc include/basic.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/basic.c -o src/basic.o

src/host_sleep.o: src/host_sleep.c include/basic.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/host_sleep.c -o src/host_sleep.o

test: all
	./$(PROG) tests/smoke.bas

clean:
	rm -f $(OBJ) $(PROG)

.PHONY: all realall clean test
