include config.mk

BIN = sfd2tf txt2ff
SRC = ${BIN:=.c}

all: ${BIN}

.c:
	@${CC} -o $@ ${CFLAGS} ${LDFLAGS} $<

clean:
	rm -f ${BIN}
