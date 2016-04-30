# Flags
CPPFLAGS = -D_POSIX_C_SOURCE=200112L
CFLAGS = -std=c89 -pedantic-errors -Wall ${CPPFLAGS}
LDFLAGS = -s

# Compiler
CC = cc
