#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpd/client.h>

//monitor dimensions
#define SCR_WIDTH 1920
#define SCR_HEIGHT 1080

#define MIN(x,y) ((x<y)?x:y)
#define MAX(x,y) ((x>y)?x:y)

void song_name();

char* message;

const unsigned int w_width = SCR_WIDTH;
const unsigned int w_height = 62;
const unsigned int w_x = 5;
unsigned int w_y = SCR_HEIGHT - 50;
const unsigned long int background = 0x00000000;

typedef struct _Fnt {
	Display *dpy;
	unsigned int h;
	XftFont *xfont;
	FcPattern *pattern;
	struct Fnt *next;
} Fnt;

typedef struct _Cmap{
	XftColor* front;
	XftColor* back;
} Cmap;

typedef struct _Drw{
	int depth;
	int screen;
	Colormap cmap;
	Drawable drawable;
	Visual* visual;
	unsigned int w;
	unsigned int h;
	Display *dpy;
	Window root;
	Window window;
	GC gc;
	XftColor *scheme;
	Fnt* font;
	Cmap* colormap;
} Drw;

Cmap colormap;

//xfont_create{{{
Fnt* xfont_create(Drw *drw, const char *fontname)
{
	Fnt *font;
	XftFont *xfont = NULL;
	FcPattern *pattern = NULL;

	if (fontname) {
		/* Using the pattern found at font->xfont->pattern does not yield the
		 * same substitution results as using the pattern returned by
		 * FcNameParse; using the latter results in the desired fallback
		 * behaviour whereas the former just results in missing-character
		 * rectangles being drawn, at least with some fonts. */
		if (!(xfont = XftFontOpenName(drw->dpy, drw->screen, fontname))) {
			fprintf(stderr, "error, cannot load font from name: '%s'\n", fontname);
			return NULL;
		}
		if (!(pattern = FcNameParse((FcChar8 *) fontname))) {
			fprintf(stderr, "error, cannot parse font name to pattern: '%s'\n", fontname);
			XftFontClose(drw->dpy, xfont);
			return NULL;
		}
	}else
		fprintf(stderr,"no font specified.");

	font = calloc(1, sizeof(Fnt));
	font->xfont = xfont;
	font->pattern = pattern;
	font->h = xfont->ascent + xfont->descent;
	font->dpy = drw->dpy;

	return font;
}
//}}}

//create_colour{{{
XftColor* create_colour(unsigned int col){
	XftColor* ret = malloc(sizeof(XftColor));
	ret->pixel = col;
	ret->color = (XRenderColor){(col&0x00ff0000) >> 8, col&0x0000ff00, (col&0x000000ff) << 8, (col&0xff000000) >> 16};
	return ret;
}
//}}}

//+/-
Drw doXorgShit(int n_lines){
	// open connection to the server
	Drw drw = {
		.dpy = XOpenDisplay(NULL),
		.depth = 32,
		.w = w_width,
		.h = w_height,
	};

	if (drw.dpy == NULL){
		fprintf(stderr, "Cannot open display\n");
		exit(1);
	}

	drw.screen = DefaultScreen(drw.dpy);
	drw.root = DefaultRootWindow(drw.dpy);
	drw.font = xfont_create(&drw, "Open Sans Semi Bold:pixelsize=20");
	drw.h = drw.font->h;
	w_y = SCR_HEIGHT - drw.h;

	//set window attributes and create window
	XVisualInfo vinfo;
	XMatchVisualInfo(drw.dpy, drw.screen, drw.depth, TrueColor, &vinfo);
	drw.visual = vinfo.visual;

	XSetWindowAttributes attr ={
		.colormap = XCreateColormap(drw.dpy, drw.root, drw.visual, AllocNone),
		.border_pixel = 0,
		.background_pixel = background,
		.override_redirect = 1,
	};
	drw.window = XCreateWindow(drw.dpy, drw.root, 0, 0, drw.w, drw.h, 0, drw.depth, InputOutput,
					drw.visual, CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &attr);
	drw.drawable = XCreatePixmap(drw.dpy, drw.root, drw.w, drw.h, drw.depth);

	//apply class name
	XClassHint class_hint ={
		.res_name = "music_widget",
		.res_class = "music_widget",
	};
	XSetClassHint(drw.dpy, drw.window, &class_hint);

	// select kind of events we are interested in
	XSelectInput(drw.dpy, drw.window, ExposureMask);

	// map (show) the window
	XMapWindow(drw.dpy, drw.window);

	drw.gc = XCreateGC (drw.dpy, drw.drawable, 0, NULL);

	XMoveResizeWindow(drw.dpy, drw.window, w_x, w_y, drw.w, drw.h);

	colormap = (Cmap){
		create_colour(0xffaaaaaa),
		create_colour(0x00000000),
	};
	drw.colormap = &colormap;

	return drw;
}

int k = 0;

unsigned int draw_text(Drw* drw, unsigned int width, const int x, const int y, const char* text){
	/*
		x,y specified in pixels, same as width
	*/
	XftDraw *d = XftDrawCreate(drw->dpy, drw->drawable, drw->visual, drw->cmap);

	//get string length
	unsigned l;
	XGlyphInfo ext;
	XftTextExtentsUtf8(drw->dpy, drw->font->xfont, (XftChar8 *)text, strlen(text), &ext);
	l = ext.xOff;
	XftDrawRect (d, drw->colormap->back, 0,0, l, drw->font->h);
	XMoveResizeWindow(drw->dpy, drw->window, SCR_WIDTH - w_x - l, w_y, l+1, drw->h);

	//draw
	XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, 0, drw->font->xfont->ascent, text, strlen(text));
	//apply changes
	XCopyArea(drw->dpy, drw->drawable, drw->window, drw->gc,0, 0, l, drw->font->h, x, y);
	return l;
}


int main(int argc, char ** argv)
{
	message = malloc(1000);
	memset(message, 0, 1000);
	Drw drw = doXorgShit(argc);

	//loop
	while(1){
		song_name();

		draw_text(&drw, 0, 0, 0, message);
		XFlush(drw.dpy); //!!!important

		usleep(1000000); //TODO: optimise for exact frame rate
		//while(QLength(drw.dpy)>0){ //fucking finally works
		//	XNextEvent(drw.dpy, &event);
		//}
	}

	// close connection to the server
	XCloseDisplay(drw.dpy);

	return 0;
 }
void song_name(){
	/* get song name */
	struct mpd_connection* conn = mpd_connection_new("localhost", 0, 0);
	mpd_send_current_song(conn);
	struct mpd_song* now_playing = mpd_recv_song(conn);

	if(now_playing){
		const char* name = mpd_song_get_tag(now_playing, MPD_TAG_TITLE, 0);
		if(name){
			memset(message, 0, 1000);
			strcpy(message, name);
			for(char* i = message; *i != 0; i++){
				*i = (*i >= 'a' && *i <= 'z') ? *i-32 : *i;
			}
		}
	}

	// cleanup
	mpd_connection_free(conn);
	if(now_playing)
		mpd_song_free(now_playing);
}