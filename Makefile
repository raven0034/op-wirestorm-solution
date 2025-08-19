CC=gcc
CFLAGS=-Wall -Wextra -g
LDFLAGS=-lpthread

EXEC=proxy
BUILDDIR=./build

HDRS=$(wildcard ./src/*.h)
SRCS=$(wildcard ./src/*.c)
BINARY=$(EXEC)
OBJS=$(SRCS:./src/%.c=$(BUILDDIR)/%.o)

.PHONY: all clean

all: $(BINARY)

clean:
	rm $(BINARY)
	rm -rf $(BUILDDIR)

$(BINARY): $(OBJS)
	echo $(OBJS)
	@echo linking $@
	$(maketargetdir)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o : ./src/%.c
	@echo compiling $<
	$(maketargetdir)
	$(CC) $(CFLAGS) $(CINCLUDES) -c -o $@ $<

define maketargetdir
	-@mkdir -p $(dir $@) > /dev/null 2>&1
endef
