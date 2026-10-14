CC ?= gcc
PKGS = gtk+-3.0
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers $(shell pkg-config --cflags $(PKGS))
LDLIBS = $(shell pkg-config --libs $(PKGS)) -lm
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)

linpaint: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDLIBS)

build/%.o: src/%.c src/paint.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build linpaint

PREFIX ?= $(HOME)/.local
install: linpaint
	install -Dm755 linpaint $(PREFIX)/bin/linpaint
	install -Dm644 linpaint.desktop $(PREFIX)/share/applications/linpaint.desktop

.PHONY: clean install
