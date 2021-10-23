SRC = music_widget.c
CC = cc
INC = /usr/include/freetype2
LDFLAGS = -lX11 -lX11-xcb -lxcb -lxcb-res -lXrender -lfontconfig -lXft -lXinerama
MPDFLAGS = -lmpdclient

music_widget: music_widget.c
	${CC} -o $@ ${SRC} -I${INC} ${LDFLAGS} ${MPDFLAGS}

run: music_widget
	./music_widget

install: music_widget
	mv -f music_widget ~/.local/bin/

mpd_test: mpd_test.c
	${CC} -o $@ mpd_test.c ${MPDFLAGS}

.PHONY: run install
