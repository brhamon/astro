CC = gcc
BUILD ?= debug
CFLAGS = -std=c99 -Wall -Werror -Wextra -D_POSIX_SOURCE -I../novasc3.1

LIBNAME = ephutil.a
SOURCES = ephutil.c moon.c strutil.c bull_a.c obs.c rootfind.c

OBJECTS = $(SOURCES:%.c=%.o)

ifeq ($(BUILD),debug)
CFLAGS += -g
else
ifeq ($(BUILD),release)
CFLAGS += -O3 -DNDEBUG
endif
endif

.PHONY: all clean

all: $(LIBNAME)

$(LIBNAME): $(OBJECTS)
	ar r $(LIBNAME) $(OBJECTS)

clean:
	@rm -f $(LIBNAME) $(OBJECTS) $(SOURCES:%.c=%.d)

### Dependencies ###

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ifneq ($(MAKECMDGOALS),clean)
include $(SOURCES:%.c=%.d)
endif

# vim: set noexpandtab tabstop=4 shiftwidth=4 softtabstop=4:
