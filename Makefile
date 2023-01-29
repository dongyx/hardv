.PHONY: targets clean install test tag

CC = cc
CFLAGS = -D_XOPEN_SOURCE=700
INSTALL = install
prefix = /usr/local
bindir = $(prefix)/bin
datarootdir = $(prefix)/share
mandir = $(datarootdir)/man
targets = hardv hardv.1

targets: $(targets)

hardv: hardv.c version LICENSE
	$(CC) $(CFLAGS) \
		-DVERSION="\"`cat version`\"" \
		-DCOPYRT="\"`grep -i 'copyright (c)' LICENSE`\"" \
		-o $@ $<

hardv.1: hardv.1.s version LICENSE
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
		chmod +x run; \
		./run; \
		cd ../..; \
	done; \
	echo all tests passed;

clean:
	@rm -rf $(targets) *.tmp
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
	if grep dev version; then \
		echo this is a dev version; \
		exit 1; \
	fi; \
	printf 'tag: '; \
	cat version; \
	git tag "v$$(cat version)"

arch:
	@set -e; \
	mkdir -p release; \
	for i in `git tag`; do \
		git archive $$i | gzip >release/hardv-$${i#v}.tar.gz; \
	done;
