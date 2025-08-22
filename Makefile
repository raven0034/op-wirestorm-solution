CC=gcc
CFLAGS=-Wall -Wextra -O3 -march=native -flto -funroll-loops -ffast-math -Wall -Wextra -g0
LDFLAGS=-s -flto

EXEC=proxy
BUILDDIR=./build

HDRS=$(wildcard ./src/*.h)
SRCS=$(wildcard ./src/*.c)
BINARY=$(EXEC)
OBJS=$(SRCS:./src/%.c=$(BUILDDIR)/%.o)

.PHONY: all clean

all: $(BINARY)

clean:
	[ -f $(BINARY) ] && rm $(BINARY)
	[ -d $(BUILDDIR) ] && rm -rf $(BUILDDIR)

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
