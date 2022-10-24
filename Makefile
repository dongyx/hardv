.PHONY: targets clean install test tag

CC = cc
CFLAGS = -std=c89 -D_POSIX_C_SOURCE=200809L -pedantic
INSTALL = install
prefix = /usr/local
bindir = $(prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
src = $(wildcard *.c)
deps = $(src:.c=.d)
objs = $(src:.c=.o)
targets = hardv hardv.1

targets: $(targets)

-include $(deps)

hardv: $(objs)
	$(CC) $(LDFLAGS) -o $@ $^

main.o: main.c version LICENSE
	$(CC) $(CFLAGS) \
		-DVERSION="\"`cat version`\"" \
		-DCOPYRT="\"`grep -i 'copyright (c)' LICENSE`\"" \
		-o $@ -c $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.d: %.c
	@$(CC) -MM $< | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' >$@

hardv.1: hardv.man1
	@echo building manpage...
	@set -e; \
	cp $< $@; \
	printf '\n.SH VERSION\n\n' >>$@; \
	printf '.nf\n' >>$@; \
	printf 'hardv ' >>$@; \
	cat version >>$@; \
	grep -i 'copyright (c)' LICENSE >>$@; \
	printf '.fi\n' >>$@;

test: $(targets)
	@set -e; \
	PATH="`pwd`:$$PATH"; \
	for i in test/*; do \
		cd $$i;	\
		echo running test $$i...; \
		./run; \
		cd ../..; \
	done; \
	echo all tests passed;

clean:
	@rm -rf $(targets) $(deps) $(objs)
	@for i in test/*; do \
		cd $$i;	\
		./clean; \
		cd ../..; \
	done
		
install: $(targets)
	@$(INSTALL) -d $(bindir)
	$(INSTALL) hardv $(bindir)
	@$(INSTALL) -d $(mandir)/man1
	$(INSTALL) hardv.1 $(mandir)/man1

tag:
	@set -e; \
	if [ "$$(git status -s)" ]; then \
		echo workspace not clean; \
		exit 1; \
	fi; \
	git tag "v$$(cat version)"

arch:
	@set -e; \
	mkdir -p release; \
	for i in `git tag`; do \
		git archive $$i | gzip >release/hardv-$${i#v}.tar.gz; \
	done;
