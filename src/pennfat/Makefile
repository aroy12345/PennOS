PROGRAM = pennfat

CFLAGS = -Wall -Werror -g

SOURCES := \
    $(wildcard *.c) \
	../util/util.c
HEADERS := \
	$(wildcard *.h) \
	../util/util.h
OBJECTS := $(patsubst */%.c, ../../bin/%.o, $(SOURCES))

$(PROGRAM): $(OBJECTS) $(HEADERS)
	clang $(OBJECTS) parser.o -o ../../bin/$(PROGRAM)

%.o: %.c $(HEADERS)
	clang $(CPPFLAGS) $(CFLAGS) -c $<