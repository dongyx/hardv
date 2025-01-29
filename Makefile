prefix = /usr/local
bindir = $(prefix)/bin
libexecdir = $(prefix)/libexec
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man

.PHONY: install clean test
all: hardv stdq hardv.1
hardv: ctab.o learn.o main.o parse.o utils.o
	$(CC) $(CFLAGS) -o hardv ctab.o learn.o main.o parse.o utils.o
stdq: stdq.c utils.c hardv.h
	$(CC) $(CFLAGS) -o stdq stdq.c utils.c
ctab.o: ctab.c hardv.h
	$(CC) -c $(CFLAGS) ctab.c
learn.o: learn.c hardv.h
	$(CC) -c -D LIBEXECDIR='"$(libexecdir)"' $(CFLAGS) learn.c
main.o: main.c hardv.h
	$(CC) -c $(CFLAGS) main.c
parse.o: parse.c hardv.h
	$(CC) -c $(CFLAGS) parse.c
utils.o: utils.c hardv.h
	$(CC) -c $(CFLAGS) utils.c
hardv.1: hardv hardv.man1
	@echo building manpage...
	@set -e; \
	cp hardv.man1 hardv.1; \
	printf '\n.SH VERSION\n\n' >>hardv.1; \
	printf '.nf\n' >>hardv.1; \
	printf 'HardV ' >>hardv.1; \
	./hardv --version | head -n1 | cut -d' ' -f2 >>hardv.1; \
	printf '.fi\n' >>hardv.1;
install: all
	mkdir -p $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(libexecdir)/hardv
	cp hardv $(DESTDIR)$(bindir)/
	cp stdq $(DESTDIR)$(libexecdir)/hardv/
	cp hardv.1 $(DESTDIR)$(mandir)/man1/
clean:
	rm -Rf *.o hardv stdq hardv.1 *.dSYM
test: all
	@set -e; \
	for i in test/*; do \
		cd $$i;	\
		echo running test $$i...; \
		chmod +x run; \
		./run; \
		cd ../..; \
	done; \
	echo all tests passed;
