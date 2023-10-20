.PHONY: all clean install test lint fuzz

CC = cc
INSTALL = install
prefix = /usr/local
bindir = $(prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
all = hardv hardv.1

all: $(all)

hardv: hardv.c
	$(CC) $(CFLAGS) -o $@ $<

hardv.1: hardv.1.s hardv
	@echo building manpage...
	@set -e; \
	cp $< $@; \
	printf '\n.SH VERSION\n\n' >>$@; \
	printf '.nf\n' >>$@; \
	printf 'HardV ' >>$@; \
	./hardv --version | head -n1 | cut -d' ' -f2 >>$@; \
	printf '.fi\n' >>$@;

test: $(all)
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

clean:
	@rm -rf $(all) lint fuzz *.tmp
	@for i in test/*; do \
		cd $$i;	\
		./clean; \
		cd ../..; \
	done
		
install: $(all)
	@$(INSTALL) -d $(bindir)
	$(INSTALL) hardv $(bindir)
	@$(INSTALL) -d $(mandir)/man1
	$(INSTALL) hardv.1 $(mandir)/man1

lint:
	@set -e; \
	rm -rf lint; \
	mkdir -p lint; \
	cp hardv.c hardv.1.s Makefile lint; \
	cd lint; \
	make \
	'CFLAGS= \
	-pedantic-errors \
	-Wextra \
	-Wno-error'

fuzz:
	@set -e; \
	rm -rf fuzz; \
	mkdir -p fuzz; \
	cp -r hardv.c hardv.1.s test Makefile fuzz; \
	cd fuzz; \
	make test \
	'CFLAGS= \
	-pedantic-errors \
	-Wextra \
	-Wno-error \
	-O3 \
	-fno-sanitize-recover \
	-fsanitize=address,undefined'
