prefix = /usr/local
bindir = $(prefix)/bin
libexecdir = $(prefix)/libexec

.PHONY: install clean test
all: hardv stdq
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
install: all
	mkdir -p $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(libexecdir)/hardv
	cp hardv $(DESTDIR)$(bindir)/
	cp stdq $(DESTDIR)$(libexecdir)/hardv/
clean:
	rm -Rf *.o hardv stdq
test: all
	@set -e; \
	PATH="`pwd`:$$PATH"; \
	for i in test/*; do \
		cd $$i;	\
		echo running test $$i...; \
		chmod +x run; \
		./run; \
		cd ../..; \
	done; \
	echo all tests passed;
