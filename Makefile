SRC = main.c xmem.h sopt.h rnd.h

install: all
	install -D 0xa0xa ${PREFIX}/bin/0xa0xa

all: 0xa0xa

clean:
	rm 0xa0xa

0xa0xa: ${SRC}
	${CC} ${CFLAGS} -lcurses main.c -o 0xa0xa
