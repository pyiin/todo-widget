#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xinerama.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LEN 10000

//monitor dimensions
#define MIN(x,y) ((x<y)?x:y)
#define MAX(x,y) ((x>y)?x:y)

void song_name();

char* message;
char* list[MAX_LEN];

XineramaScreenInfo screen;
unsigned int w_width = 1920;
unsigned int w_height = 1080;
unsigned int w_x = 30;
unsigned int w_y = 60;
const unsigned long int background = 0x00000000;

typedef struct _Fnt {
	Display *dpy;
	unsigned int h;
	XftFont *xfont;
} Fnt;

typedef struct _Cmap{
	XftColor* front;
	XftColor* back;
	XftColor* warn;
	XftColor* urgn;
	XftColor* good;
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

//xinerama {{{
void run_Xinerama(int screen_num, Drw* drw){
	int n_screens;
	XineramaScreenInfo* xsinfo = XineramaQueryScreens(drw->dpy, &n_screens);
#ifdef __DEBUG
	for(int i=0; i<n_screens; i++){
		fprintf(stderr, "[%d] = %dx%d+%d+%d\n",i, xsinfo[i].width, xsinfo[i].height, xsinfo[i].x_org, xsinfo[i].y_org);
	}
	fprintf(stderr, "using screen number %d\n", screen_num < n_screens ? screen_num : 0);
#endif
	screen = xsinfo[screen_num < n_screens ? screen_num : 0];
	XFree(xsinfo);
	//w_y = screen.height+screen.y_org - drw->font->xfont->ascent - 7;
	//w_x = screen.width+screen.x_org;
}
//}}}

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
	drw.font = xfont_create(&drw, "Ubuntu Mono Nerd Font:pixelsize=35");//Open Sans Semi Bold
	drw.h = drw.font->h;

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

	run_Xinerama(screen_num, &drw);
	//apply class name
	XClassHint class_hint ={
		.res_name = "todo_widget",
		.res_class = "todo_widget",
	};
	XSetClassHint(drw.dpy, drw.window, &class_hint);

	// select kind of events we are interested in
	XSelectInput(drw.dpy, drw.window, ExposureMask);

	// map (show) the window
	XMapWindow(drw.dpy, drw.window);

	drw.gc = XCreateGC (drw.dpy, drw.drawable, 0, NULL);

	XMoveResizeWindow(drw.dpy, drw.window, w_x, w_y, drw.w, drw.h);

	colormap = (Cmap){
		create_colour(0xffbbbbaa), //front
		create_colour(0x00000000), //back
		create_colour(0xfffabd2f), //warn
		create_colour(0xfffb491d), //urgn
		create_colour(0xffb8bb24), //good
	};
	drw.colormap = &colormap;

	XLowerWindow(drw.dpy, drw.window);

	return drw;
}

int k = 0;

unsigned int draw_text(Drw* drw, const int x, const int y, const char** text, int N){
	/*
		x,y specified in pixels
	*/

	XftDraw *d = XftDrawCreate(drw->dpy, drw->drawable, drw->visual, drw->cmap);

	//get string length
	unsigned l = 0;
	XGlyphInfo ext;
	int i=0;
	int f_h = drw->font->h;
	while(i<N){
		XftTextExtentsUtf8(drw->dpy, drw->font->xfont, (XftChar8 *)text[i], strlen(text[i]), &ext);
		l = MAX(l, ext.xOff);
		XftDrawRect (d, drw->colormap->back, 0, 0, l, f_h);

	//draw
		switch(text[i][0]){
			case '!':
				XftDrawStringUtf8(d, drw->colormap->urgn, drw->font->xfont, 0, drw->font->xfont->ascent, (const FcChar8 *)text[i]+1, strlen(text[i]) - 2);
				break;
			case '?':
				XftDrawStringUtf8(d, drw->colormap->warn, drw->font->xfont, 0, drw->font->xfont->ascent, (const FcChar8 *)text[i]+1, strlen(text[i]) - 2);
				break;
			case '.':
				XftDrawStringUtf8(d, drw->colormap->good, drw->font->xfont, 0, drw->font->xfont->ascent, (const FcChar8 *)text[i]+1, strlen(text[i]) - 2);
				break;
			default:
				XftDrawStringUtf8(d, drw->colormap->front, drw->font->xfont, 0, drw->font->xfont->ascent, (const FcChar8 *)text[i], strlen(text[i]) - 1);
		}
		//apply changes
		XCopyArea(drw->dpy, drw->drawable, drw->window, drw->gc,0, 0, l, drw->font->h, x, y+i*f_h);
		i++;
	}
	drw->h = (i <=0 ? 1 : i)*f_h;
	return l;
}

int update_list(char* f_name){
	FILE* f = fopen(f_name, "r");
	if(f == NULL){
		list[0] = malloc(MAX_LEN);
		strcpy(list[0], "File `");
		strcat(list[0], f_name);
		strcat(list[0], "` does not exist ");
		return 1;
	}
	int fail = 0;
	size_t L = 1;
	int N=0;
	for(int i=0;i<MAX_LEN && fail != -1 && L > 0; i++){
		fail = getline(&list[i], &L, f);
		N =i;
	}
	fclose(f);
	return N;
}


int main(int argc, char ** argv)
{
	char* filename = malloc(MAX_LEN);
	int scr_num = 0;
	if(argc > 2) scr_num = atoi(argv[2]);
	if(argc > 1) strcpy(filename, argv[1]);
	else return 1;
	Drw drw = doXorgShit(scr_num);
	//loop
	int count = 0;
	while(1){
		count++;
		// update text;
		int c = update_list(filename);
		unsigned l = draw_text(&drw, 0, 0, list, c);
#ifdef __DEBUG
		fprintf(stderr, "%dx%d+%d+%d\n", screen.width + screen.x_org - l, w_y, l+1, drw.h);
#endif
		XMoveResizeWindow(drw.dpy, drw.window, w_x , w_y, l+1, drw.h);
		XFlush(drw.dpy); //!!!important

		if((count & 0x1f) == 0)
			run_Xinerama(scr_num, &drw);
		usleep(1000000);
	}
	// close connection to the server
	XCloseDisplay(drw.dpy);

	return 0;
 }
