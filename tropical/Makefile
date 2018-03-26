CC = gcc
BUILD ?= debug

NOVAS = novas.a
NOVASPATH = ../Cdist
NOVASLIB = $(NOVASPATH)/$(NOVAS)
EPHUT = ephutil.a
EPHUTPATH = ../ephutil
EPHUTLIB = $(EPHUTPATH)/$(EPHUT)

CFLAGS = -Wall -Wextra -I $(NOVASPATH) -I $(EPHUTPATH)
LDFLAGS = -lm -lcurl

TARGET = tropical
SOURCES = tropical.c

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

$(TARGET): $(OBJECTS) $(NOVASLIB) $(EPHUTLIB)

$(NOVASLIB):
	make -C $(NOVASPATH) $(NOVAS)

$(EPHUTLIB):
	make -C $(EPHUTPATH) $(EPHUT)

clean:
	-rm -f $(TARGET) $(OBJECTS) $(SOURCES:%.c=%.d)
	make -C ../Cdist clean
	make -C ../ephutil clean

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
