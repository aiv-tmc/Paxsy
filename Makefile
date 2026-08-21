CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -pedantic -I. -I./src
LDFLAGS  :=
OBJDIR   := obj
BINDIR   := .

# Source files: all .c files under src/, excluding main.c and libmain.c
SRCS     := $(shell find src -name '*.c')
COMMON_SRCS := $(filter-out src/main.c src/libmain.c, $(SRCS))
COMMON_OBJS := $(patsubst src/%.c, $(OBJDIR)/%.o, $(COMMON_SRCS))
MAIN_OBJ := $(OBJDIR)/main.o
LIBMAIN_OBJ := $(OBJDIR)/libmain.o

# Default is 1 (build).
BUILD_LIB ?= 1

.PHONY: all clean

all: paxsy

# Rule to create object directories
$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# paxsy executable
paxsy: $(MAIN_OBJ) $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -rf $(OBJDIR) paxsy
