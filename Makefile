PREFIX=/usr/local/bin
CFLAGS=-Wextra -std=c99

bin2iso: bin2iso.c

install: bin2iso
	install -Dm 755 bin2iso $(DESTDIR)$(PREFIX)/bin2iso

clean:
	$(RM) bin2iso

.PHONY: install clean
