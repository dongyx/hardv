.PHONY: targets clean install

CC = cc
CFLAGS = -std=c89 -D_POSIX_C_SOURCE=200809L -pedantic
INSTALL = install
prefix = /usr/local
bindir = $(prefix)/bin
src = $(wildcard *.c)
deps = $(src:.c=.d)
objs = $(src:.c=.o)
targets = hardv

targets: $(targets)

-include $(deps)

hardv: $(objs)
	$(CC) $(LDFLAGS) -o $@ $^

main.o: main.c version
	$(CC) $(CFLAGS) -DVERSION="`cat version`" -o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.d: %.c
	$(CC) -MM $< | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' >$@

clean:
	rm -rf $(targets) $(deps) $(objs)

install: $(targets)
	$(INSTALL) -d $(bindir)
	$(INSTALL) hardv $(bindir)
