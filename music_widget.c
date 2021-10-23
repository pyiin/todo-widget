#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xinerama.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mpd/client.h>

//monitor dimensions
#define MIN(x,y) ((x<y)?x:y)
#define MAX(x,y) ((x>y)?x:y)

void song_name();

char* message;

XineramaScreenInfo screen;
unsigned int w_width = 1920;
unsigned int w_height = 62;
unsigned int w_x = 5;
unsigned int w_y = 1080 - 30;
const unsigned long int background = 0x00000000;

typedef struct _Fnt {
	Display *dpy;
	unsigned int h;
	XftFont *xfont;
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
	}else
		fprintf(stderr,"no font specified.");

	font = calloc(1, sizeof(Fnt));
	font->xfont = xfont;
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
Drw doXorgShit(int screen_num){
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
	drw.font = xfont_create(&drw, "Baloo 2 semibold:pixelsize=20");//Open Sans Semi Bold
	drw.h = drw.font->h;

	//xinerama {{{
	int n_screens;
	XineramaScreenInfo* xsinfo = XineramaQueryScreens(drw.dpy, &n_screens);
	for(int i=0; i<n_screens; i++){
		fprintf(stderr, "[%d] = %dx%d+%d+%d\n",i, xsinfo[i].width, xsinfo[i].height, xsinfo[i].x_org, xsinfo[i].y_org);
	}
	fprintf(stderr, "using screen number %d\n", screen_num < n_screens ? screen_num : 0);
	screen = xsinfo[screen_num < n_screens ? screen_num : 0];
	XFree(xsinfo);
	//}}}

	w_y = screen.height+screen.y_org - drw.font->xfont->ascent - 7;
	w_x = screen.width+screen.x_org;


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

unsigned int draw_text(Drw* drw, const int x, const int y, const char* text){
	/*
		x,y specified in pixels
	*/

	XftDraw *d = XftDrawCreate(drw->dpy, drw->drawable, drw->visual, drw->cmap);

	//get string length
	unsigned l;
	XGlyphInfo ext;
	XftTextExtentsUtf8(drw->dpy, drw->font->xfont, (XftChar8 *)text, strlen(text), &ext);
	l = ext.xOff;
	XftDrawRect (d, drw->colormap->back, 0,0, l, drw->font->h);

	//draw
	XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, 0, drw->font->xfont->ascent, text, strlen(text));
	//apply changes
	XCopyArea(drw->dpy, drw->drawable, drw->window, drw->gc,0, 0, l, drw->font->h, x, y);
	return l;
}


int main(int argc, char ** argv)
{
	int scr_num = 0;
	if(argc > 1) scr_num = atoi(argv[1]);
	Drw drw = doXorgShit(scr_num);
	message = malloc(1);

	//loop
	while(1){
		song_name();
		unsigned l = draw_text(&drw, 0, 0, message);
#ifdef __DEBUG
		fprintf(stderr, "%dx%d+%d+%d\n", screen.width + screen.x_org - l, w_y, l+1, drw.h);
#endif
		XMoveResizeWindow(drw.dpy, drw.window, screen.width + screen.x_org - l - 1, w_y, l+1, drw.h);
		XFlush(drw.dpy); //!!!important

		usleep(100000); //TODO: wait for song to change
	}
	// close connection to the server
	XCloseDisplay(drw.dpy);

	return 0;
 }

void song_name(){
	/* get song name */
	struct mpd_connection* conn = mpd_connection_new("localhost", 0, 0);
	struct mpd_status* status = mpd_run_status(conn); // has to be second, i don't know why
	mpd_send_current_song(conn);
	struct mpd_song* now_playing = mpd_recv_song(conn);

	// ----- get pause
	enum mpd_state state = mpd_status_get_state(status);
	mpd_status_free(status);
	// -----

	if(now_playing && state == MPD_STATE_PLAY){
		const char* name = mpd_song_get_tag(now_playing, MPD_TAG_TITLE, 0);
		const char* artist = mpd_song_get_tag(now_playing, MPD_TAG_ARTIST, 0);
		if(name){
			//should probably change to use realloc
			free(message);
			message = malloc(strlen(name)+strlen(artist)+4);
			strcpy(message, artist);
			strcat(message, " - ");
			strcat(message, name);
		}
	}
	else{
#ifdef __DEBUG
		printf("NO %d\n", state);
#endif
		message = malloc(1);
		*message = 0;
	}

	// cleanup
	mpd_connection_free(conn);
	if(now_playing)
		mpd_song_free(now_playing);
}
