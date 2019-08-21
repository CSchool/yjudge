CFLAGS=-Wall -Wextra -pedantic -g3 -O0
PREFIX=/usr/local

all: yjudge-runner yjudge-inject.so

install: all
	install -D -g root -o root -m 4755 yjudge-runner $(PREFIX)/bin/yjudge-runner
	install -D yjudge-inject.so $(PREFIX)/lib/yjudge-inject.so

yjudge-runner: yjudge-runner.c config.h
	$(CC) $(CFLAGS) yjudge-runner.c -o $@

yjudge-inject.so: yjudge-inject.c config.h
	$(CC) $(CFLAGS) -shared -fPIC -Wl,-init,yjudge_inject yjudge-inject.c -o $@

clean:
	rm -f yjudge-runner

.PHONY: all clean install
