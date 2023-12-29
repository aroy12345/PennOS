
# NOTE: parser.o should go into the bin folder

PROGRAM = pennos

CFLAGS = -Wall -Werror -g

SOURCES := \
    $(wildcard src/util/*.c) \
    $(wildcard src/kernel/*.c) \
    $(filter-out src/pennfat/pennfat.c, $(wildcard src/pennfat/*.c)) \
    $(wildcard src/shell/*.c) \
    $(wildcard src/filesystem/*.c) \
    $(wildcard src/logger/*.c) \
    src/pennos.c
HEADERS := \
	$(wildcard src/util/*.h) \
    $(wildcard src/kernel/*.h) \
    $(wildcard src/pennfat/*.h) \
    $(wildcard src/shell/*.h) \
	$(wildcard src/filesystem/*.h) \
	$(wildcard src/logger/*.h)
OBJECTS := $(patsubst */src/%.c, bin/%.o, $(SOURCES))

$(PROGRAM): $(OBJECTS) $(HEADERS)
	clang $(OBJECTS) bin/parser.o -o bin/$(PROGRAM)

%.o: %.c $(HEADERS)
	clang $(CPPFLAGS) $(CFLAGS) -c $<
