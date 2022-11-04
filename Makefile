SRC = todo_desktop.c
CC = cc
INC = /usr/include/freetype2
LDFLAGS = -lX11 -lX11-xcb -lxcb -lxcb-res -lXrender -lfontconfig -lXft -lXinerama
DFLAGS = -fsanitize=address

todo_desktop: todo_desktop.c
	${CC} -o $@ ${SRC} -I${INC} ${LDFLAGS}

todo_desktop_dbg: todo_desktop.c
	${CC} -o $@ ${SRC} -I${INC} ${LDFLAGS} ${DFLAGS} -D__DEBUG

run: todo_desktop
	./todo_desktop

install: todo_desktop
	mv -f todo_desktop ~/.local/bin/

mpd_test: mpd_test.c
	${CC} -o $@ mpd_test.c ${MPDFLAGS}

.PHONY: run install
