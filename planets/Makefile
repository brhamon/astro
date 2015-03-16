CC = gcc
BUILD ?= debug
CFLAGS = -Wall -Wextra -I../Cdist
LDFLAGS = -lm -lgfortran -lcurl

NOVASLIB = ../Cdist/novas.a
TARGET = planets
SOURCES = planets.c bull_a.c

OBJECTS = $(SOURCES:%.c=%.o)

ifeq ($(BUILD),debug)
CFLAGS += -g
else
ifeq ($(BUILD),release)
CFLAGS += -O3 -DNDEBUG
endif
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(NOVASLIB)

clean:
	@rm -f $(TARGET) $(OBJECTS) $(SOURCES:%.c=%.d)

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
