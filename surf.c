/* See LICENSE file for copyright and license details.
 *
 * To understand surf, start reading main().
 */
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <JavaScriptCore/JavaScript.h>
#include <webkit2/webkit2.h>
#include <X11/X.h>
#include <X11/Xatom.h>

/* use main(int argc, char *argv[]) */
#define ARGBEGIN	for (argv0 = *argv, argv++, argc--;\
					argv[0] && argv[0][0] == '-'\
					&& argv[0][1];\
					argc--, argv++) {\
				char argc_;\
				char **argv_;\
				int brk_;\
				if (argv[0][1] == '-' && argv[0][2] == '\0') {\
					argv++;\
					argc--;\
					break;\
				}\
				for (brk_ = 0, argv[0]++, argv_ = argv;\
						argv[0][0] && !brk_;\
						argv[0]++) {\
					if (argv_ != argv)\
						break;\
					argc_ = argv[0][0];\
					switch (argc_)
#define ARGEND			}\
			}

#define ARGC()		argc_

#define EARGF(x)	((argv[0][1] == '\0' && argv[1] == NULL)?\
				((x), abort(), (char *)0) :\
				(brk_ = 1, (argv[0][1] != '\0')?\
					(&argv[0][1]) :\
					(argc--, argv++, argv[0])))

#define ARGF()		((argv[0][1] == '\0' && argv[1] == NULL)?\
				(char *)0 :\
				(brk_ = 1, (argv[0][1] != '\0')?\
					(&argv[0][1]) :\
					(argc--, argv++, argv[0])))

#define MSGBUFSZ 8

#define LENGTH(x)			   (sizeof(x) / sizeof(x[0]))
#define CLEANMASK(mask)		 (mask & (MODKEY|GDK_SHIFT_MASK))

regex_t *filter_expressions;

enum { AtomFind, AtomGo, AtomUri, AtomUTF8, AtomLast };

enum {
	OnDoc   = WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT,
	OnLink  = WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK,
	OnImg   = WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE,
	OnMedia = WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA,
	OnEdit  = WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE,
	OnBar   = WEBKIT_HIT_TEST_RESULT_CONTEXT_SCROLLBAR,
	OnSel   = WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION,
	OnAny   = OnDoc | OnLink | OnImg | OnMedia | OnEdit | OnBar | OnSel,
};

typedef enum {
	AccessMicrophone,
	AccessWebcam,
	CaretBrowsing,
	Certificate,
	CookiePolicies,
	DarkMode,
	DiskCache,
	DefaultCharset,
	DNSPrefetch,
	Ephemeral,
	FileURLsCrossAccess,
	FontSize,
	FrameFlattening,
	Geolocation,
	HideBackground,
	Inspector,
	Java,
	JavaScript,
	KioskMode,
	LoadImages,
	MediaManualPlay,
	PreferredLanguages,
	RunInFullscreen,
	ScrollBars,
	ShowIndicators,
	SiteQuirks,
	SmoothScrolling,
	SpellChecking,
	SpellLanguages,
	StrictTLS,
	Style,
	WebGL,
	ZoomLevel,
	ParameterLast
} ParamName;

typedef union {
	int i;
	float f;
	const void *v;
} Arg;

typedef struct {
	Arg val;
	int prio;
} Parameter;

typedef struct Client {
	GtkWidget *win;
	WebKitWebView *view;
	WebKitSettings *settings;
	WebKitWebContext *context;
	WebKitWebInspector *inspector;
	WebKitFindController *finder;
	WebKitHitTestResult *mousepos;
	GTlsCertificate *cert, *failedcert;
	GTlsCertificateFlags tlserr;
	Window xid;
	guint64 pageid;
	int progress, fullscreen, https, insecure, errorpage;
	const char *title, *overtitle, *targeturi;
	const char *needle;
	struct Client *next;
	GList *tabs;
	guint selected_tab;
	GtkWidget *tab_bar;
	int tab_click_index; 
	bool tab_drag;
	pid_t subprocess_pid;
} Client;

typedef struct {
	guint mod;
	guint keyval;
	void (*func)(Client *c, const Arg *a);
	const Arg arg;
} Key;

typedef struct {
	unsigned int target;
	unsigned int mask;
	guint button;
	void (*func)(Client *c, const Arg *a, WebKitHitTestResult *h);
	const Arg arg;
	unsigned int stopevent;
} Button;

typedef struct {
	char *token;
	char *uri;
} SearchEngine;

typedef struct {
	const char *uri;
	Parameter config[ParameterLast];
	regex_t re;
} UriParameters;

typedef struct {
	char *regex;
	char *file;
	regex_t re;
} SiteSpecific;

typedef struct {
	char *title;
	char *uri;
	bool suspended;
} Tab;

/* Surf */
static void usage(void);
static void setup(void);
static void sigchld(int unused);
static void sighup(int unused);
static char *buildfile(const char *path);
static char *buildpath(const char *path);
static char *untildepath(const char *path);
static const char *getuserhomedir(const char *user);
static const char *getcurrentuserhomedir(void);
static Client *newclient(Client *c);
static void loaduri(Client *c, const Arg *a);
static const char *geturi(Client *c);
static void setatom(Client *c, int a, const char *v);
static const char *getatom(Client *c, int a);
static void updatetitle(Client *c);
static void gettogglestats(Client *c);
static void getpagestats(Client *c);
static WebKitCookieAcceptPolicy cookiepolicy_get(void);
static char cookiepolicy_set(const WebKitCookieAcceptPolicy p);
static void seturiparameters(Client *c, const char *uri, ParamName *params);
static void setparameter(Client *c, int refresh, ParamName p, const Arg *a);
static const char *getcert(const char *uri);
static void setcert(Client *c, const char *file);
static const char *getstyle(const char *uri);
static void setstyle(Client *c, const char *file);
static void runscript(Client *c);
static void evalscript(Client *c, const char *jsstr, ...);
static void updatewinid(Client *c);
static void handleplumb(Client *c, const char *uri);
static void newwindow(Client *c, const Arg *a, int noembed);
static void spawn(Client *c, const Arg *a);
static void msgext(Client *c, char type, const Arg *a);
static void destroyclient(Client *c);
static void cleanup(void);
static int insertmode = 0;

/* GTK/WebKit */
static WebKitWebView *newview(Client *c, WebKitWebView *rv);
static void initwebextensions(WebKitWebContext *wc, Client *c);
static GtkWidget *createview(WebKitWebView *v, WebKitNavigationAction *a,
							 Client *c);
static gboolean buttonreleased(GtkWidget *w, GdkEvent *e, Client *c);
static GdkFilterReturn processx(GdkXEvent *xevent, GdkEvent *event,
								gpointer d);
static gboolean winevent(GtkWidget *w, GdkEvent *e, Client *c);
static gboolean readsock(GIOChannel *s, GIOCondition ioc, gpointer unused);
static void showview(WebKitWebView *v, Client *c);
static GtkWidget *createwindow(Client *c);
static gboolean loadfailedtls(WebKitWebView *v, gchar *uri,
							  GTlsCertificate *cert,
							  GTlsCertificateFlags err, Client *c);
static void loadchanged(WebKitWebView *v, WebKitLoadEvent e, Client *c);
static void progresschanged(WebKitWebView *v, GParamSpec *ps, Client *c);
static void titlechanged(WebKitWebView *view, GParamSpec *ps, Client *c);
static void mousetargetchanged(WebKitWebView *v, WebKitHitTestResult *h,
							   guint modifiers, Client *c);
static gboolean permissionrequested(WebKitWebView *v,
									WebKitPermissionRequest *r, Client *c);
static gboolean decidepolicy(WebKitWebView *v, WebKitPolicyDecision *d,
							 WebKitPolicyDecisionType dt, Client *c);
static void decidenavigation(WebKitPolicyDecision *d, Client *c);
static void decidenewwindow(WebKitPolicyDecision *d, Client *c);
static void decideresource(WebKitPolicyDecision *d, Client *c);
static void insecurecontent(WebKitWebView *v, WebKitInsecureContentEvent e,
							Client *c);
static gboolean viewusrmsgrcv(WebKitWebView *v, WebKitUserMessage *m,
							  gpointer u);
static void webprocessterminated(WebKitWebView *v,
								 WebKitWebProcessTerminationReason r,
								 Client *c);
static void closeview(WebKitWebView *v, Client *c);
static void destroywin(GtkWidget* w, Client *c);
static gchar *parseuri(const gchar *uri);

/* Hotkeys */
static void pasteuri(GtkClipboard *clipboard, const char *text, gpointer d);
static void reload(Client *c, const Arg *a);
static void print(Client *c, const Arg *a);
static void showcert(Client *c, const Arg *a);
static void clipboard(Client *c, const Arg *a);
static void zoom(Client *c, const Arg *a);
static void scrollv(Client *c, const Arg *a);
static void scrollh(Client *c, const Arg *a);
static void navigate(Client *c, const Arg *a);
static void stop(Client *c, const Arg *a);
static void quit(Client *c, const Arg *a);
static void toggle(Client *c, const Arg *a);
static void togglefullscreen(Client *c, const Arg *a);
static void togglecookiepolicy(Client *c, const Arg *a);
static void toggleinspector(Client *c, const Arg *a);
static void find(Client *c, const Arg *a);
static void insert(Client *c, const Arg *a);

/* filter url */
static bool filter_init(void);
static bool filter_request(const gchar *uri);

/* Buttons */
static void clicknavigate(Client *c, const Arg *a, WebKitHitTestResult *h);
static void clicknewwindow(Client *c, const Arg *a, WebKitHitTestResult *h);
static void clicknewtab(Client *c, const Arg *a, WebKitHitTestResult *h);
static void clickexternplayer(Client *c, const Arg *a, WebKitHitTestResult *h);

/* external player */
static void playexternal(Client *c, const Arg *a);

/* download-console */
static void downloadstarted(WebKitWebContext *wc, WebKitDownload *d,
							Client *c);
static void downloadfailed(WebKitDownload *d, GParamSpec *ps, void *arg);
static void downloadfinished(WebKitDownload *d, GParamSpec *ps, void *arg);
static gboolean decidedestination(WebKitDownload *d,
								  gchar *suggested_filename, void *arg);
static void printprogress(WebKitDownload *d, GParamSpec *ps, void *arg);
static void logdownload(WebKitDownload *d, gchar *tail);
static void spawndls(Client *c, const Arg *a);

/* tab_bar */
static void free_tab(Tab *tab);
static void add_tab(Client *client, const gchar *uri);
static void remove_tab(Client *client, Tab *tab);
static void update_tab_bar(Client *c);
static void free_all_tabs(Client *client);
static void reload_tab(Client *c);
static void switch_tab(Client *c, const Arg *a);
static void move_tab(Client *c, const Arg *a);
static void update_tab_uri(Client *c);
static void update_tab_title(Client *c);
static void unsuspend_tab(Client *c);
static void suspend_tab(Client *c);
static void close_tab(Client *c, const Arg *a);
static void new_tab(Client *c, const Arg *a);
static int  get_font_size(GtkWidget *widget);
static void tab_bar_click(Client *c, bool close);
static void fill_tab_bar(Client *c);
static void create_tab_bar(Client *c);


static char winid[64];
static char togglestats[11];
static char pagestats[2];
static Atom atoms[AtomLast];
static Window embed;
static int showxid;
static int cookiepolicy;
static Display *dpy;
static Client *clients;
static GdkDevice *gdkkb;
static char *stylefile;
static const char *useragent;
static Parameter *curconfig;
static int modparams[ParameterLast];
static int spair[2];
char *argv0;

static ParamName loadtransient[] = {
	Certificate,
	CookiePolicies,
	DiskCache,
	DNSPrefetch,
	FileURLsCrossAccess,
	JavaScript,
	LoadImages,
	PreferredLanguages,
	ShowIndicators,
	StrictTLS,
	ParameterLast
};

static ParamName loadcommitted[] = {
//	AccessMicrophone,
//	AccessWebcam,
	CaretBrowsing,
	DarkMode,
	DefaultCharset,
	FontSize,
	FrameFlattening,
	Geolocation,
	HideBackground,
	Inspector,
	Java,
//	KioskMode,
	MediaManualPlay,
	RunInFullscreen,
	ScrollBars,
	SiteQuirks,
	SmoothScrolling,
	SpellChecking,
	SpellLanguages,
	Style,
	ZoomLevel,
	ParameterLast
};

static ParamName loadfinished[] = {
	ParameterLast
};

/* configuration, allows nested code to access above variables */
#include "config.h"

// Function to free a tab
void free_tab(Tab *tab) {
	g_free(tab->title);
	g_free(tab->uri);
	g_free(tab);
	//intentional_error();
}

// Function to add a tab to the clients tab list
void add_tab(Client *client, const gchar *uri) {
	Tab *tab = g_malloc(sizeof(Tab));
	tab->title = g_strdup(uri);
	tab->uri = g_strdup(uri);
	tab->suspended = false;
	client->tabs = g_list_append(client->tabs, tab);
}

// Function to remove a tab from the clients tab list
void remove_tab(Client *client, Tab *tab) {
	client->tabs = g_list_remove(client->tabs, tab);
	free_tab(tab);
}

void update_tab_bar(Client *c) {
	gtk_grid_remove_row(GTK_GRID(c->tab_bar), 0);
	fill_tab_bar(c);
}


// Function to free all tabs
void free_all_tabs(Client *client) {
	g_list_free_full(client->tabs, (GDestroyNotify) free_tab);
	client->tabs = NULL;
}

void reload_tab(Client *c) {
	char *uri;
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	if (selected_tab->suspended) {
		uri = "about:blank";
	} else {
		uri = selected_tab->uri;
	}
	Arg tab_arg = {.v = uri };
	loaduri(c, &tab_arg);
}

void switch_tab(Client *c, const Arg *a) {
	int new_selected_tab = MIN(MAX(c->selected_tab + a->i, 0), g_list_length(c->tabs) - 1);
	
	if (c->selected_tab == new_selected_tab) {
		return;
	}
	c->selected_tab = new_selected_tab;
	update_tab_bar(c);
	suspend_tab(c);
}

void move_tab(Client *c, const Arg *a) {
	int new_index = c->selected_tab + a->i;
	int tab_count = g_list_length(c->tabs);

	// Ensure the new index is within valid bounds
	if (new_index >= 0 && new_index < tab_count) {
		// Get the nodes for the current and the target positions
		GList *current_node = g_list_nth(c->tabs, c->selected_tab);
		GList *target_node = g_list_nth(c->tabs, new_index);

		// Swap the data of the two nodes
		if (current_node != NULL && target_node != NULL) {
			gpointer temp_data = current_node->data;
			current_node->data = target_node->data;
			target_node->data = temp_data;
		}

		// Update the selected tab index
		c->selected_tab = new_index;
	}

	update_tab_bar(c);
}

void update_tab_uri(Client *c) {
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	if (selected_tab->suspended) {
		return;
	}
	const char *uri = geturi(c);
	selected_tab->uri = g_strdup(uri);
}

void update_tab_title(Client *c) {
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	if (selected_tab->suspended) {
		return;
	}
	const char *title = c->title ? c->title : "untitled";
	
	selected_tab->title = g_strdup(title);
	
	update_tab_uri(c);
	update_tab_bar(c);
}

void unsuspend_tab(Client *c) {
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	selected_tab->suspended = false;
	reload_tab(c);
}

void suspend_tab(Client *c) {
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	selected_tab->suspended = true;
	reload_tab(c);
}

void close_tab(Client *c, const Arg *a) {
	if (g_list_length(c->tabs) == 1) {
		g_print("tried to close last tab\n");
		return;
	}
	
	int index = a->i;
	if (index >= g_list_length(c->tabs) && index != -1) {
		g_print("tried to close tab that does not exist\n");
		return;
	}
	
	if (index == -1 || index == c->selected_tab) {
		remove_tab(c, g_list_nth_data(c->tabs, c->selected_tab));
	} else {
		remove_tab(c, g_list_nth_data(c->tabs, index));
	}
	
	if (index <= c->selected_tab && c->selected_tab > 0) {
		c->selected_tab -= 1;
		suspend_tab(c);
	}
	
	update_tab_bar(c);
}

void new_tab(Client *c, const Arg *a) {
	add_tab(c, "about:blank");
	c->selected_tab = g_list_length(c->tabs) - 1;
	update_tab_bar(c);
	reload_tab(c);
	Arg arg = SETPROP("_SURF_URI", "_SURF_GO", PROMPT_GO);
	spawn(c, &arg);
}

int get_font_size(GtkWidget *widget) {
	GtkStyleContext *style_context = gtk_widget_get_style_context(widget);
	PangoFontDescription *font_desc;
	gtk_style_context_get (style_context,
					   gtk_style_context_get_state (style_context),
					   GTK_STYLE_PROPERTY_FONT, &font_desc,
					   NULL);

	int size = pango_font_description_get_size(font_desc);

	// Convert size from Pango units to points
	size = size / PANGO_SCALE;

	return size;
}

void tab_mouse_pos_index(GdkEvent *e, Client *c, int *index, bool *close_tab) {
	GdkEventButton *event_button = (GdkEventButton *)e;
	double x = event_button->x;
	
	GtkAllocation allocation;
	gtk_widget_get_allocation(GTK_WIDGET(c->win), &allocation);
	int width = allocation.width;
	
	int n_tabs = g_list_length(c->tabs);
	int tab_width = width / MAX(n_tabs + 1, min_tab_fraction_size);
	
	*index = (int)(x / tab_width);
	
	if (close_tab != NULL) {
		int x_width = MIN(get_font_size(c->tab_bar) * 5 / 2, tab_width / 4);
		*close_tab = (x - *index * tab_width > tab_width - x_width && *index < n_tabs);
	}
}

void tab_bar_mouse_press(GtkWidget *w, GdkEvent *e, Client *c) {
	if (c->tab_click_index != -1) {
		return;
	}
	
	tab_mouse_pos_index(e, c, &c->tab_click_index, NULL);
	c->tab_drag = false;
}

void tab_bar_mouse_release(GtkWidget *w, GdkEvent *e, Client *c) {
	if (c->tab_click_index == -1) {
		return;
	}
	
	int press_index;
	bool close_tab_flag;
	tab_mouse_pos_index(e, c, &press_index, &close_tab_flag);
	
	if (!c->tab_drag) {
		tab_bar_click(c, close_tab_flag);
	}
	
	c->tab_click_index = -1;
	
	update_tab_bar(c);
}

void tab_bar_mouse_move(GtkWidget *w, GdkEvent *e, Client *c) {
	if (c->tab_click_index == -1) {
		return;
	}
	int move_index;
	tab_mouse_pos_index(e, c, &move_index, NULL);
	
	if (move_index == c->tab_click_index) {
		return;
	}
	
	if (move_index < 0 || move_index >= g_list_length(c->tabs) ||
		c->tab_click_index < 0 || c->tab_click_index >= g_list_length(c->tabs)) {
		return;
	}
	
	GList *node1 = g_list_nth(c->tabs, c->tab_click_index);
	GList *node2 = g_list_nth(c->tabs, move_index);
	
	if (node1 && node2) {
		GList *temp = node1->data;
		node1->data = node2->data;
		node2->data = temp;
	}
	
	if (c->selected_tab == move_index) {
		c->selected_tab = c->tab_click_index;
	} else if (c->selected_tab == c->tab_click_index) {
		c->selected_tab = move_index;
	} 
	
	c->tab_click_index = move_index;
	c->tab_drag = true;
	update_tab_bar(c);
}

void tab_bar_click(Client *c, bool close) {
	if (g_list_length(c->tabs) == 0) {
		return;
	}
	
	int status;
    pid_t result = waitpid(c->subprocess_pid, &status, WNOHANG);
	
	if (result == 0 && c->subprocess_pid != 0) {
		return;
	}
	
	if (close) {
		Arg close_arg = {.i = c->tab_click_index};
		close_tab(c, &close_arg);
		return;
	}
	
	int n_tabs = g_list_length(c->tabs);
	
	if (c->tab_click_index >= n_tabs) {
		Arg new_tab_arg = {0};
		new_tab(c, &new_tab_arg);
		return;
	}
	
	if (c->selected_tab == c->tab_click_index) {
		return;
	}
	
	c->selected_tab = c->tab_click_index;
	update_tab_bar(c);
	suspend_tab(c);
}

void fill_tab_bar(Client *c) {
	GdkRGBA fg_color, light_color;
	gdk_rgba_parse(&fg_color, tab_bar_color[1]);
	gdk_rgba_parse(&light_color, tab_bar_color[2]);
	
	int tab_index = 0;
	// Add tabs to the tab bar
	for (GList *l = c->tabs; l != NULL; l = l->next) {
		Tab *tab = (Tab *)l->data;
		
		gchar *padded_title = g_strdup_printf("  %s", tab->title);
		GtkWidget *label = gtk_label_new(padded_title);
		g_free(padded_title);
		
		gtk_label_set_xalign(GTK_LABEL(label), 0.0);
		gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
		
		gtk_widget_set_size_request(label, -1, tab_bar_height);
		
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END); //if too long cut it
		
		GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start(GTK_BOX(tab_box), label, TRUE, TRUE, 0);
		
		GtkWidget *close_button = gtk_label_new("  ⨯  ");
		gtk_widget_set_halign(close_button, GTK_ALIGN_END);
		gtk_box_pack_start(GTK_BOX(tab_box), close_button, FALSE, FALSE, 0);
		
		if (tab_index == c->selected_tab) {
			gtk_widget_override_background_color(tab_box, GTK_STATE_FLAG_NORMAL, &fg_color);
		} else if (tab_index == c->tab_click_index) {
			gtk_widget_override_background_color(tab_box, GTK_STATE_FLAG_NORMAL, &light_color);
		}
		
		gtk_grid_attach_next_to(GTK_GRID(c->tab_bar), tab_box, NULL, GTK_POS_RIGHT, 1, 1);  // Pack the box into the grid
		gtk_widget_show_all(tab_box);
		
		tab_index++;
	}
	
	GtkWidget *new_tab = gtk_label_new("  +"); 
	gtk_label_set_xalign(GTK_LABEL(new_tab), 0.0);
	gtk_grid_attach_next_to(GTK_GRID(c->tab_bar), new_tab, NULL, GTK_POS_RIGHT, 1, 1);
	
	gtk_widget_show(new_tab);
	
	for (int i = g_list_length(c->tabs) + 1; i < min_tab_fraction_size; i++) {
		GtkWidget *spacing = gtk_label_new("");  // Create an empty label as a spacer
		gtk_grid_attach_next_to(GTK_GRID(c->tab_bar), spacing, NULL, GTK_POS_RIGHT, 1, 1);
		gtk_widget_show(spacing);
	}
}

void create_tab_bar(Client *c) {
	GdkRGBA bg_color;
	gdk_rgba_parse(&bg_color, tab_bar_color[0]);

	c->tab_bar = gtk_grid_new(); 
	gtk_widget_override_background_color(c->tab_bar, GTK_STATE_FLAG_NORMAL, &bg_color);
	gtk_widget_set_size_request(c->tab_bar, -1, tab_bar_height);  // Set the height of the black bar
	gtk_grid_set_column_homogeneous(GTK_GRID(c->tab_bar), true);
	
	fill_tab_bar(c);	
}

void
die(const char *errstr, ...)
{
	   va_list ap;

	   va_start(ap, errstr);
	   vfprintf(stderr, errstr, ap);
	   va_end(ap);
	   exit(1);
}

static bool
filter_init(void) {
	bool errors = false;
	char *errorbuf;

	errorbuf = malloc(sizeof(char) * BUFSIZ);
	filter_expressions = malloc(sizeof(regex_t) * LENGTH(filter_patterns));

	for (off_t idx = 0; idx < LENGTH(filter_patterns); idx++) {
		char *pat = filter_patterns[idx];
		int err = regcomp(&filter_expressions[idx], pat,
							REG_EXTENDED | REG_ICASE | REG_NOSUB);
		if (err != 0) {
			/* regerror always ends messages with 0x00 */
			(void) regerror(err, &filter_expressions[idx], errorbuf, BUFSIZ);
			fprintf(stderr, "Failed to compile \"%s\": %s\n", pat, errorbuf);
			errors = true;
		}
	}

	free(errorbuf);
	return !errors;
}

static bool
filter_request(const gchar *uri) {
	if (!strcmp(uri, "about:blank"))
		return false;
	for (off_t idx = 0; idx < LENGTH(filter_patterns); idx++) {
		if (regexec(&filter_expressions[idx], uri, 0, NULL, 0) == REG_NOMATCH) {
			continue;
		}
#ifdef FILTER_VERBOSE
		fprintf(stderr, "filtering \"%s\"\n", uri);
#endif
		return true;
	}
#ifdef FILTER_VERBOSE
	fprintf(stderr, "not filtering \"%s\"\n", uri);
#endif
	return false;
}

void
usage(void)
{
	die("usage: surf [-bBdDfFgGiIkKmMnNpPsStTvwxX]\n"
		"[-a cookiepolicies ] [-c cookiefile] [-C stylefile] [-e xid]\n"
		"[-r scriptfile] [-u useragent] [-z zoomlevel] [uri]\n");
}

void
setup(void)
{
	GIOChannel *gchanin;
	GdkDisplay *gdpy;
	int i, j;

	/* clean up any zombies immediately */
	sigchld(0);
	if (signal(SIGHUP, sighup) == SIG_ERR)
		die("Can't install SIGHUP handler");

	if (!(dpy = XOpenDisplay(NULL)))
		die("Can't open default display");

	/* atoms */
	atoms[AtomFind] = XInternAtom(dpy, "_SURF_FIND", False);
	atoms[AtomGo] = XInternAtom(dpy, "_SURF_GO", False);
	atoms[AtomUri] = XInternAtom(dpy, "_SURF_URI", False);
	atoms[AtomUTF8] = XInternAtom(dpy, "UTF8_STRING", False);

	gtk_init(NULL, NULL);

	gdpy = gdk_display_get_default();

	curconfig = defconfig;

	/* dirs and files */
	cookiefile = buildfile(cookiefile);
	scriptfile = buildfile(scriptfile);
	certdir	= buildpath(certdir);
	dlstatus   = buildpath(dlstatus);
	dldir	  = buildpath(dldir);
	if (curconfig[Ephemeral].val.i)
		cachedir = NULL;
	else
		cachedir   = buildpath(cachedir);

	gdkkb = gdk_seat_get_keyboard(gdk_display_get_default_seat(gdpy));

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, spair) < 0) {
		fputs("Unable to create sockets\n", stderr);
		spair[0] = spair[1] = -1;
	} else {
		gchanin = g_io_channel_unix_new(spair[0]);
		g_io_channel_set_encoding(gchanin, NULL, NULL);
		g_io_channel_set_flags(gchanin, g_io_channel_get_flags(gchanin)
							   | G_IO_FLAG_NONBLOCK, NULL);
		g_io_channel_set_close_on_unref(gchanin, TRUE);
		g_io_add_watch(gchanin, G_IO_IN, readsock, NULL);
	}


	for (i = 0; i < LENGTH(certs); ++i) {
		if (!regcomp(&(certs[i].re), certs[i].regex, REG_EXTENDED)) {
			certs[i].file = g_strconcat(certdir, "/", certs[i].file,
										NULL);
		} else {
			fprintf(stderr, "Could not compile regex: %s\n",
					certs[i].regex);
			certs[i].regex = NULL;
		}
	}

	if (!stylefile) {
		styledir = buildpath(styledir);
		for (i = 0; i < LENGTH(styles); ++i) {
			if (!regcomp(&(styles[i].re), styles[i].regex,
				REG_EXTENDED)) {
				styles[i].file = g_strconcat(styledir, "/",
									styles[i].file, NULL);
			} else {
				fprintf(stderr, "Could not compile regex: %s\n",
						styles[i].regex);
				styles[i].regex = NULL;
			}
		}
		g_free(styledir);
	} else {
		stylefile = buildfile(stylefile);
	}

	for (i = 0; i < LENGTH(uriparams); ++i) {
		if (regcomp(&(uriparams[i].re), uriparams[i].uri,
			REG_EXTENDED)) {
			fprintf(stderr, "Could not compile regex: %s\n",
					uriparams[i].uri);
			uriparams[i].uri = NULL;
			continue;
		}

		/* copy default parameters with higher priority */
		for (j = 0; j < ParameterLast; ++j) {
			if (defconfig[j].prio >= uriparams[i].config[j].prio)
				uriparams[i].config[j] = defconfig[j];
		}
	}

	if (!filter_init()) {
		die("Failed to compile one or more filter expressions\n");
	}
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("Can't install SIGCHLD handler");
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
}

void
sighup(int unused)
{
	Arg a = { .i = 0 };
	Client *c;

	for (c = clients; c; c = c->next)
		reload(c, &a);
}

char *
buildfile(const char *path)
{
	char *dname, *bname, *bpath, *fpath;
	FILE *f;

	dname = g_path_get_dirname(path);
	bname = g_path_get_basename(path);

	bpath = buildpath(dname);
	g_free(dname);

	fpath = g_build_filename(bpath, bname, NULL);
	g_free(bpath);
	g_free(bname);

	if (!(f = fopen(fpath, "a")))
		die("Could not open file: %s\n", fpath);

	g_chmod(fpath, 0600); /* always */
	fclose(f);

	return fpath;
}

static const char*
getuserhomedir(const char *user)
{
	struct passwd *pw = getpwnam(user);

	if (!pw)
		die("Can't get user %s login information.\n", user);

	return pw->pw_dir;
}

static const char*
getcurrentuserhomedir(void)
{
	const char *homedir;
	const char *user;
	struct passwd *pw;

	homedir = getenv("HOME");
	if (homedir)
		return homedir;

	user = getenv("USER");
	if (user)
		return getuserhomedir(user);

	pw = getpwuid(getuid());
	if (!pw)
		die("Can't get current user home directory\n");

	return pw->pw_dir;
}

char *
buildpath(const char *path)
{
	char *apath, *fpath;

	if (path[0] == '~')
		apath = untildepath(path);
	else
		apath = g_strdup(path);

	/* creating directory */
	if (g_mkdir_with_parents(apath, 0700) < 0)
		die("Could not access directory: %s\n", apath);

	fpath = realpath(apath, NULL);
	g_free(apath);

	return fpath;
}

char *
untildepath(const char *path)
{
	   char *apath, *name, *p;
	   const char *homedir;

	   if (path[1] == '/' || path[1] == '\0') {
			   p = (char *)&path[1];
			   homedir = getcurrentuserhomedir();
	   } else {
			   if ((p = strchr(path, '/')))
					   name = g_strndup(&path[1], p - (path + 1));
			   else
					   name = g_strdup(&path[1]);

			   homedir = getuserhomedir(name);
			   g_free(name);
	   }
	   apath = g_build_filename(homedir, p, NULL);
	   return apath;
}

Client *
newclient(Client *rc)
{
	Client *c;

	if (!(c = calloc(1, sizeof(Client))))
		die("Cannot malloc!\n");

	c->next = clients;
	clients = c;

	c->progress = 100;
	
	add_tab(c, "about:blank");
	c->selected_tab = 0;
	c->tab_click_index = -1;
	
	create_tab_bar(c);
	
	c->view = newview(c, rc ? rc->view : NULL);

	return c;
}

void
loaduri(Client *c, const Arg *a)
{
	struct stat st;
	char *url, *path, *apath;
	const char *uri = a->v;

	if (g_strcmp0(uri, "") == 0)
		return;
	
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	if (selected_tab->suspended) {
		webkit_web_view_load_alternate_html(c->view, suspended_html, uri, NULL);
		return;
	}

	if (g_strcmp0(uri, keybinds_url) == 0) {
		webkit_web_view_load_alternate_html(c->view, keybinds_html, uri, NULL);
		return;
	}

	if (g_str_has_prefix(uri, "http://")  ||
		g_str_has_prefix(uri, "https://") ||
		g_str_has_prefix(uri, "file://")  ||
		g_str_has_prefix(uri, "about:")) {
		url = g_strdup(uri);
	} else {
		if (uri[0] == '~')
			apath = untildepath(uri);
		else
			apath = (char *)uri;
		if (!stat(apath, &st) && (path = realpath(apath, NULL))) {
			url = g_strdup_printf("file://%s", path);
			free(path);
		} else {
			url = parseuri(uri);
		}
		if (apath != uri)
			free(apath);
	}

	setatom(c, AtomUri, url);

	if (strcmp(url, geturi(c)) == 0 && strcmp(url, "about:blank") != 0) {
		reload(c, a);
	} else {
		webkit_web_view_load_uri(c->view, url);
		updatetitle(c);
	}
	
	g_free(url);
}

const char *
geturi(Client *c)
{
	const char *uri;

	if (!(uri = webkit_web_view_get_uri(c->view)))
		uri = "about:blank";
	return uri;
}

void
setatom(Client *c, int a, const char *v)
{
	XChangeProperty(dpy, c->xid,
					atoms[a], atoms[AtomUTF8], 8, PropModeReplace,
					(unsigned char *)v, strlen(v) + 1);
	XSync(dpy, False);
}

const char *
getatom(Client *c, int a)
{
	static char buf[BUFSIZ];
	Atom adummy;
	int idummy;
	unsigned long ldummy;
	unsigned char *p = NULL;

	XSync(dpy, False);
	XGetWindowProperty(dpy, c->xid,
					   atoms[a], 0L, BUFSIZ, False, atoms[AtomUTF8],
					   &adummy, &idummy, &ldummy, &ldummy, &p);
	if (p)
		strncpy(buf, (char *)p, LENGTH(buf) - 1);
	else
		buf[0] = '\0';
	XFree(p);

	return buf;
}

void
updatetitle(Client *c)
{
	char *title;
	const char *name = c->overtitle ? c->overtitle :
					   c->title ? c->title : "";

	if (curconfig[ShowIndicators].val.i) {
		gettogglestats(c);
		getpagestats(c);

		if (c->progress != 100)
			title = g_strdup_printf("[%i%%] %s:%s | %s",
					c->progress, togglestats, pagestats, name);
		else
			title = g_strdup_printf("%s:%s | %s",
					togglestats, pagestats, name);
		
		gtk_window_set_title(GTK_WINDOW(c->win), title);
		g_free(title);
	} else {
		gtk_window_set_title(GTK_WINDOW(c->win), name);
	}
	
	update_tab_title(c);
}

void
gettogglestats(Client *c)
{
	togglestats[0] = cookiepolicy_set(cookiepolicy_get());
	togglestats[1] = curconfig[CaretBrowsing].val.i ?   'C' : 'c';
	togglestats[2] = curconfig[Geolocation].val.i ?	 'G' : 'g';
	togglestats[3] = curconfig[DiskCache].val.i ?	   'D' : 'd';
	togglestats[4] = curconfig[LoadImages].val.i ?	  'I' : 'i';
	togglestats[5] = curconfig[JavaScript].val.i ?	  'S' : 's';
	togglestats[6] = curconfig[Style].val.i ?		   'M' : 'm';
	togglestats[7] = curconfig[FrameFlattening].val.i ? 'F' : 'f';
	togglestats[8] = curconfig[Certificate].val.i ?	 'X' : 'x';
	togglestats[9] = curconfig[StrictTLS].val.i ?	   'T' : 't';
}

void
getpagestats(Client *c)
{
	if (c->https)
		pagestats[0] = (c->tlserr || c->insecure) ?  'U' : 'T';
	else
		pagestats[0] = '-';
	pagestats[1] = '\0';
}

WebKitCookieAcceptPolicy
cookiepolicy_get(void)
{
	switch (((char *)curconfig[CookiePolicies].val.v)[cookiepolicy]) {
	case 'a':
		return WEBKIT_COOKIE_POLICY_ACCEPT_NEVER;
	case '@':
		return WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;
	default: /* fallthrough */
	case 'A':
		return WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
	}
}

char
cookiepolicy_set(const WebKitCookieAcceptPolicy p)
{
	switch (p) {
	case WEBKIT_COOKIE_POLICY_ACCEPT_NEVER:
		return 'a';
	case WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY:
		return '@';
	default: /* fallthrough */
	case WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS:
		return 'A';
	}
}

void
seturiparameters(Client *c, const char *uri, ParamName *params)
{
	Parameter *config, *uriconfig = NULL;
	int i, p;

	for (i = 0; i < LENGTH(uriparams); ++i) {
		if (uriparams[i].uri &&
			!regexec(&(uriparams[i].re), uri, 0, NULL, 0)) {
			uriconfig = uriparams[i].config;
			break;
		}
	}

	curconfig = uriconfig ? uriconfig : defconfig;

	for (i = 0; (p = params[i]) != ParameterLast; ++i) {
		switch(p) {
		default: /* FALLTHROUGH */
			if (!(defconfig[p].prio < curconfig[p].prio ||
				defconfig[p].prio < modparams[p]))
				continue;
		case Certificate:
		case CookiePolicies:
		case Style:
			setparameter(c, 0, p, &curconfig[p].val);
		}
	}
}

void
setparameter(Client *c, int refresh, ParamName p, const Arg *a)
{
	GdkRGBA bgcolor = { 0 };

	modparams[p] = curconfig[p].prio;

	switch (p) {
	case AccessMicrophone:
		return; /* do nothing */
	case AccessWebcam:
		return; /* do nothing */
	case CaretBrowsing:
		webkit_settings_set_enable_caret_browsing(c->settings, a->i);
		refresh = 0;
		break;
	case Certificate:
		if (a->i)
			setcert(c, geturi(c));
		return; /* do not update */
	case CookiePolicies:
		webkit_cookie_manager_set_accept_policy(
			webkit_web_context_get_cookie_manager(c->context),
			cookiepolicy_get());
		refresh = 0;
		break;
	case DarkMode:
		g_object_set(gtk_settings_get_default(),
					 "gtk-application-prefer-dark-theme", a->i, NULL);
		return;
	case DiskCache:
		webkit_web_context_set_cache_model(c->context, a->i ?
			WEBKIT_CACHE_MODEL_WEB_BROWSER :
			WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
		return; /* do not update */
	case DefaultCharset:
		webkit_settings_set_default_charset(c->settings, a->v);
		return; /* do not update */
	case DNSPrefetch:
		webkit_settings_set_enable_dns_prefetching(c->settings, a->i);
		return; /* do not update */
	case FileURLsCrossAccess:
		webkit_settings_set_allow_file_access_from_file_urls(
			c->settings, a->i);
		webkit_settings_set_allow_universal_access_from_file_urls(
			c->settings, a->i);
		return; /* do not update */
	case FontSize:
		webkit_settings_set_default_font_size(c->settings, a->i);
		return; /* do not update */
	case FrameFlattening:
		webkit_settings_set_enable_frame_flattening(c->settings, a->i);
		break;
	case Geolocation:
		refresh = 0;
		break;
	case HideBackground:
		if (a->i)
			webkit_web_view_set_background_color(c->view, &bgcolor);
		return; /* do not update */
	case Inspector:
		webkit_settings_set_enable_developer_extras(c->settings, a->i);
		return; /* do not update */
	case Java:
		webkit_settings_set_enable_java(c->settings, a->i);
		return; /* do not update */
	case JavaScript:
		webkit_settings_set_enable_javascript(c->settings, a->i);
		break;
	case KioskMode:
		return; /* do nothing */
	case LoadImages:
		webkit_settings_set_auto_load_images(c->settings, a->i);
		break;
	case MediaManualPlay:
		webkit_settings_set_media_playback_requires_user_gesture(
			c->settings, a->i);
		break;
	case PreferredLanguages:
		return; /* do nothing */
	case RunInFullscreen:
		return; /* do nothing */
	case ScrollBars:
		/* Disabled until we write some WebKitWebExtension for
		 * manipulating the DOM directly.
		enablescrollbars = !enablescrollbars;
		evalscript(c, "document.documentElement.style.overflow = '%s'",
			enablescrollbars ? "auto" : "hidden");
		*/
		return; /* do not update */
	case ShowIndicators:
		break;
	case SmoothScrolling:
		webkit_settings_set_enable_smooth_scrolling(c->settings, a->i);
		return; /* do not update */
	case SiteQuirks:
		webkit_settings_set_enable_site_specific_quirks(
			c->settings, a->i);
		break;
	case SpellChecking:
		webkit_web_context_set_spell_checking_enabled(
			c->context, a->i);
		return; /* do not update */
	case SpellLanguages:
		return; /* do nothing */
	case StrictTLS:
		webkit_web_context_set_tls_errors_policy(c->context, a->i ?
			WEBKIT_TLS_ERRORS_POLICY_FAIL :
			WEBKIT_TLS_ERRORS_POLICY_IGNORE);
		break;
	case Style:
		webkit_user_content_manager_remove_all_style_sheets(
			webkit_web_view_get_user_content_manager(c->view));
		if (a->i)
			setstyle(c, getstyle(geturi(c)));
		refresh = 0;
		break;
	case WebGL:
		webkit_settings_set_enable_webgl(c->settings, a->i);
		break;
	case ZoomLevel:
		webkit_web_view_set_zoom_level(c->view, a->f);
		return; /* do not update */
	default:
		return; /* do nothing */
	}

	updatetitle(c);
	if (refresh)
		reload(c, a);
}

const char *
getcert(const char *uri)
{
	int i;

	for (i = 0; i < LENGTH(certs); ++i) {
		if (certs[i].regex &&
			!regexec(&(certs[i].re), uri, 0, NULL, 0))
			return certs[i].file;
	}

	return NULL;
}

void
setcert(Client *c, const char *uri)
{
	const char *file = getcert(uri);
	char *host;
	GTlsCertificate *cert;

	if (!file)
		return;

	if (!(cert = g_tls_certificate_new_from_file(file, NULL))) {
		fprintf(stderr, "Could not read certificate file: %s\n", file);
		return;
	}

	if ((uri = strstr(uri, "https://"))) {
		uri += sizeof("https://") - 1;
		host = g_strndup(uri, strchr(uri, '/') - uri);
		webkit_web_context_allow_tls_certificate_for_host(c->context,
			cert, host);
		g_free(host);
	}

	g_object_unref(cert);

}

const char *
getstyle(const char *uri)
{
	int i;

	if (stylefile)
		return stylefile;

	for (i = 0; i < LENGTH(styles); ++i) {
		if (styles[i].regex &&
			!regexec(&(styles[i].re), uri, 0, NULL, 0))
			return styles[i].file;
	}

	return "";
}

void
setstyle(Client *c, const char *file)
{
	gchar *style;

	if (!g_file_get_contents(file, &style, NULL, NULL)) {
		fprintf(stderr, "Could not read style file: %s\n", file);
		return;
	}

	webkit_user_content_manager_add_style_sheet(
		webkit_web_view_get_user_content_manager(c->view),
		webkit_user_style_sheet_new(style,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL, NULL));

	g_free(style);
}

void
runscript(Client *c)
{
	gchar *script;
	gsize l;

	if (g_file_get_contents(scriptfile, &script, &l, NULL) && l)
		evalscript(c, "%s", script);
	g_free(script);
}

void
evalscript(Client *c, const char *jsstr, ...)
{
	va_list ap;
	gchar *script;

	va_start(ap, jsstr);
	script = g_strdup_vprintf(jsstr, ap);
	va_end(ap);

	webkit_web_view_run_javascript(c->view, script, NULL, NULL, NULL);
	g_free(script);
}

void
updatewinid(Client *c)
{
	snprintf(winid, LENGTH(winid), "%lu", c->xid);
}

void
handleplumb(Client *c, const char *uri)
{
	Arg a = (Arg)PLUMB(uri);
	spawn(c, &a);
}

void
newwindow(Client *c, const Arg *a, int noembed)
{
	int i = 0;
	char tmp[64];
	const char *cmd[29], *uri;
	const Arg arg = { .v = cmd };

	cmd[i++] = argv0;
	cmd[i++] = "-a";
	cmd[i++] = curconfig[CookiePolicies].val.v;
	cmd[i++] = curconfig[ScrollBars].val.i ? "-B" : "-b";
	if (cookiefile && g_strcmp0(cookiefile, "")) {
		cmd[i++] = "-c";
		cmd[i++] = cookiefile;
	}
	if (stylefile && g_strcmp0(stylefile, "")) {
		cmd[i++] = "-C";
		cmd[i++] = stylefile;
	}
	cmd[i++] = curconfig[DiskCache].val.i ? "-D" : "-d";
	if (embed && !noembed) {
		cmd[i++] = "-e";
		snprintf(tmp, LENGTH(tmp), "%lu", embed);
		cmd[i++] = tmp;
	}
	cmd[i++] = curconfig[RunInFullscreen].val.i ? "-F" : "-f" ;
	cmd[i++] = curconfig[Geolocation].val.i ?	 "-G" : "-g" ;
	cmd[i++] = curconfig[LoadImages].val.i ?	  "-I" : "-i" ;
	cmd[i++] = curconfig[KioskMode].val.i ?	   "-K" : "-k" ;
	cmd[i++] = curconfig[Style].val.i ?		   "-M" : "-m" ;
	cmd[i++] = curconfig[Inspector].val.i ?	   "-N" : "-n" ;
	if (scriptfile && g_strcmp0(scriptfile, "")) {
		cmd[i++] = "-r";
		cmd[i++] = scriptfile;
	}
	cmd[i++] = curconfig[JavaScript].val.i ? "-S" : "-s";
	cmd[i++] = curconfig[StrictTLS].val.i ? "-T" : "-t";
	if (fulluseragent && g_strcmp0(fulluseragent, "")) {
		cmd[i++] = "-u";
		cmd[i++] = fulluseragent;
	}
	if (showxid)
		cmd[i++] = "-w";
	cmd[i++] = curconfig[Certificate].val.i ? "-X" : "-x" ;
	/* do not keep zoom level */
	cmd[i++] = "--";
	if ((uri = a->v))
		cmd[i++] = uri;
	cmd[i] = NULL;

	spawn(c, &arg);
}

void
spawn(Client *c, const Arg *a)
{
	c->subprocess_pid = fork();
	if (c->subprocess_pid == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		close(spair[0]);
		close(spair[1]);
		setsid();
		execvp(((char **)a->v)[0], (char **)a->v);
		fprintf(stderr, "%s: execvp %s", argv0, ((char **)a->v)[0]);
		perror(" failed");
		exit(1);
	}
}

void
destroyclient(Client *c)
{
	Client *p;

	webkit_web_view_stop_loading(c->view);
	/* Not needed, has already been called
	gtk_widget_destroy(c->win);
	 */

	for (p = clients; p && p->next != c; p = p->next)
		;
	if (p)
		p->next = c->next;
	else
		clients = c->next;
	free_all_tabs(c);
	free(c);
}

void
cleanup(void)
{
	while (clients)
		destroyclient(clients);

	close(spair[0]);
	close(spair[1]);
	g_free(cookiefile);
	g_free(scriptfile);
	g_free(stylefile);
	g_free(cachedir);
	g_free(dldir);
	g_free(dlstatus);
	XCloseDisplay(dpy);
}

WebKitWebView *
newview(Client *c, WebKitWebView *rv)
{
	WebKitWebView *v;
	WebKitSettings *settings;
	WebKitWebContext *context;
	WebKitCookieManager *cookiemanager;
	WebKitUserContentManager *contentmanager;

	/* Webview */
	if (rv) {
		v = WEBKIT_WEB_VIEW(webkit_web_view_new_with_related_view(rv));
		context = webkit_web_view_get_context(v);
		settings = webkit_web_view_get_settings(v);
	} else {
		settings = webkit_settings_new_with_settings(
		   "allow-file-access-from-file-urls", curconfig[FileURLsCrossAccess].val.i,
		   "allow-universal-access-from-file-urls", curconfig[FileURLsCrossAccess].val.i,
		   "auto-load-images", curconfig[LoadImages].val.i,
		   "default-charset", curconfig[DefaultCharset].val.v,
		   "default-font-size", curconfig[FontSize].val.i,
		   "enable-caret-browsing", curconfig[CaretBrowsing].val.i,
		   "enable-developer-extras", curconfig[Inspector].val.i,
		   "enable-dns-prefetching", curconfig[DNSPrefetch].val.i,
		   "enable-frame-flattening", curconfig[FrameFlattening].val.i,
		   "enable-html5-database", curconfig[DiskCache].val.i,
		   "enable-html5-local-storage", curconfig[DiskCache].val.i,
		   "enable-java", curconfig[Java].val.i,
		   "enable-javascript", curconfig[JavaScript].val.i,
		   "enable-site-specific-quirks", curconfig[SiteQuirks].val.i,
		   "enable-smooth-scrolling", curconfig[SmoothScrolling].val.i,
		   "enable-webgl", curconfig[WebGL].val.i,
		   "media-playback-requires-user-gesture", curconfig[MediaManualPlay].val.i,
		   NULL);
/* For more interesting settings, have a look at
 * http://webkitgtk.org/reference/webkit2gtk/stable/WebKitSettings.html */

		if (strcmp(fulluseragent, "")) {
			webkit_settings_set_user_agent(settings, fulluseragent);
		} else if (surfuseragent) {
			webkit_settings_set_user_agent_with_application_details(
				settings, "Surf", VERSION);
		}
		useragent = webkit_settings_get_user_agent(settings);

		contentmanager = webkit_user_content_manager_new();

		if (curconfig[Ephemeral].val.i) {
			context = webkit_web_context_new_ephemeral();
		} else {
			context = webkit_web_context_new_with_website_data_manager(
					  webkit_website_data_manager_new(
					  "base-cache-directory", cachedir,
					  "base-data-directory", cachedir,
					  NULL));
		}


		cookiemanager = webkit_web_context_get_cookie_manager(context);

		/* rendering process model, can be a shared unique one
		 * or one for each view */
		webkit_web_context_set_process_model(context,
			WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);
		/* TLS */
		webkit_web_context_set_tls_errors_policy(context,
			curconfig[StrictTLS].val.i ? WEBKIT_TLS_ERRORS_POLICY_FAIL :
			WEBKIT_TLS_ERRORS_POLICY_IGNORE);
		/* disk cache */
		webkit_web_context_set_cache_model(context,
			curconfig[DiskCache].val.i ? WEBKIT_CACHE_MODEL_WEB_BROWSER :
			WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);

		/* Currently only works with text file to be compatible with curl */
		if (!curconfig[Ephemeral].val.i)
			webkit_cookie_manager_set_persistent_storage(cookiemanager,
				cookiefile, WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
		/* cookie policy */
		webkit_cookie_manager_set_accept_policy(cookiemanager,
			cookiepolicy_get());
		/* languages */
		webkit_web_context_set_preferred_languages(context,
			curconfig[PreferredLanguages].val.v);
		webkit_web_context_set_spell_checking_languages(context,
			curconfig[SpellLanguages].val.v);
		webkit_web_context_set_spell_checking_enabled(context,
			curconfig[SpellChecking].val.i);

		g_signal_connect(G_OBJECT(context), "download-started",
						 G_CALLBACK(downloadstarted), c);
		g_signal_connect(G_OBJECT(context), "initialize-web-extensions",
						 G_CALLBACK(initwebextensions), c);

		v = g_object_new(WEBKIT_TYPE_WEB_VIEW,
			"settings", settings,
			"user-content-manager", contentmanager,
			"web-context", context,
			NULL);
	}

	g_signal_connect(G_OBJECT(v), "notify::estimated-load-progress",
			 G_CALLBACK(progresschanged), c);
	g_signal_connect(G_OBJECT(v), "notify::title",
			 G_CALLBACK(titlechanged), c);
	g_signal_connect(G_OBJECT(v), "button-release-event",
			 G_CALLBACK(buttonreleased), c);
	g_signal_connect(G_OBJECT(v), "close",
			G_CALLBACK(closeview), c);
	g_signal_connect(G_OBJECT(v), "create",
			 G_CALLBACK(createview), c);
	g_signal_connect(G_OBJECT(v), "decide-policy",
			 G_CALLBACK(decidepolicy), c);
	g_signal_connect(G_OBJECT(v), "insecure-content-detected",
			 G_CALLBACK(insecurecontent), c);
	g_signal_connect(G_OBJECT(v), "load-failed-with-tls-errors",
			 G_CALLBACK(loadfailedtls), c);
	g_signal_connect(G_OBJECT(v), "load-changed",
			 G_CALLBACK(loadchanged), c);
	g_signal_connect(G_OBJECT(v), "mouse-target-changed",
			 G_CALLBACK(mousetargetchanged), c);
	g_signal_connect(G_OBJECT(v), "permission-request",
			 G_CALLBACK(permissionrequested), c);
	g_signal_connect(G_OBJECT(v), "ready-to-show",
			 G_CALLBACK(showview), c);
	g_signal_connect(G_OBJECT(v), "user-message-received",
			 G_CALLBACK(viewusrmsgrcv), c);
	g_signal_connect(G_OBJECT(v), "web-process-terminated",
			 G_CALLBACK(webprocessterminated), c);
			

	c->context = context;
	c->settings = settings;

	setparameter(c, 0, DarkMode, &curconfig[DarkMode].val);

	return v;
}

static gboolean
readsock(GIOChannel *s, GIOCondition ioc, gpointer unused)
{
	static char msg[MSGBUFSZ];
	GError *gerr = NULL;
	gsize msgsz;

	if (g_io_channel_read_chars(s, msg, sizeof(msg), &msgsz, &gerr) !=
		G_IO_STATUS_NORMAL) {
		if (gerr) {
			fprintf(stderr, "surf: error reading socket: %s\n",
					gerr->message);
			g_error_free(gerr);
		}
		return TRUE;
	}
	if (msgsz < 2) {
		fprintf(stderr, "surf: message too short: %lu\n", (unsigned long)msgsz);
		return TRUE;
	}

	return TRUE;
}

void
initwebextensions(WebKitWebContext *wc, Client *c)
{
	webkit_web_context_set_web_extensions_directory(wc, WEBEXTDIR);
}

GtkWidget *
createview(WebKitWebView *v, WebKitNavigationAction *a, Client *c)
{
	Client *n;

	switch (webkit_navigation_action_get_navigation_type(a)) {
	case WEBKIT_NAVIGATION_TYPE_OTHER: /* fallthrough */
		/*
		 * popup windows of type “other” are almost always triggered
		 * by user gesture, so inverse the logic here
		 */
/* instead of this, compare destination uri to mouse-over uri for validating window */
		if (webkit_navigation_action_is_user_gesture(a))
			return NULL;
	case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_RELOAD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
		n = newclient(c);
		break;
	default:
		return NULL;
	}

	return GTK_WIDGET(n->view);
}

gboolean
buttonreleased(GtkWidget *w, GdkEvent *e, Client *c)
{
	WebKitHitTestResultContext element;
	int i;
	
	Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
	if (selected_tab->suspended) {
		unsuspend_tab(c);
		return TRUE;
	}

	element = webkit_hit_test_result_get_context(c->mousepos);

	for (i = 0; i < LENGTH(buttons); ++i) {
		if (element & buttons[i].target &&
			e->button.button == buttons[i].button &&
			CLEANMASK(e->button.state) == CLEANMASK(buttons[i].mask) &&
			buttons[i].func) {
			buttons[i].func(c, &buttons[i].arg, c->mousepos);
			return buttons[i].stopevent;
		}
	}

	return FALSE;
}

GdkFilterReturn
processx(GdkXEvent *e, GdkEvent *event, gpointer d)
{
	Client *c = (Client *)d;
	XPropertyEvent *ev;
	Arg a;
	
	if (((XEvent *)e)->type == PropertyNotify) {
		ev = &((XEvent *)e)->xproperty;
		if (ev->state == PropertyNewValue) {
			if (ev->atom == atoms[AtomFind]) {
				find(c, NULL);
				
				return GDK_FILTER_REMOVE;
			} else if (ev->atom == atoms[AtomGo]) {
				a.v = getatom(c, AtomGo);
				
				Tab *selected_tab = g_list_nth_data(c->tabs, c->selected_tab);
				selected_tab->suspended = false;
				
				loaduri(c, &a);

				return GDK_FILTER_REMOVE;
			}
		}
	}
	return GDK_FILTER_CONTINUE;
}

gboolean
winevent(GtkWidget *w, GdkEvent *e, Client *c)
{
	int i;

	switch (e->type) {
	case GDK_ENTER_NOTIFY:
		c->overtitle = c->targeturi;
		updatetitle(c);
		break;
	case GDK_KEY_PRESS:
	if (!curconfig[KioskMode].val.i &&
			!insertmode ||
			CLEANMASK(e->key.state) == (MODKEY|GDK_SHIFT_MASK) ||
			CLEANMASK(e->key.state) == (MODKEY) ||
			gdk_keyval_to_lower(e->key.keyval) == (GDK_KEY_Escape)) {			for (i = 0; i < LENGTH(keys); ++i) {
				if (gdk_keyval_to_lower(e->key.keyval) ==
					keys[i].keyval &&
					CLEANMASK(e->key.state) == keys[i].mod &&
					keys[i].func) {
					updatewinid(c);
					keys[i].func(c, &(keys[i].arg));
					return TRUE;
				}
			}
		}
	case GDK_LEAVE_NOTIFY:
		c->overtitle = NULL;
		updatetitle(c);
		break;
	case GDK_WINDOW_STATE:
		if (e->window_state.changed_mask ==
			GDK_WINDOW_STATE_FULLSCREEN)
			c->fullscreen = e->window_state.new_window_state &
							GDK_WINDOW_STATE_FULLSCREEN;
		break;
	default:
		break;
	}

	return FALSE;
}

void
showview(WebKitWebView *v, Client *c)
{
	GdkWindow *gwin;

	// Create the black bar using the separate function
	GdkRGBA fg_color, bg_color;
	gdk_rgba_parse(&fg_color, tab_bar_color[1]);
	gdk_rgba_parse(&bg_color, tab_bar_color[0]);

	GtkWidget *tab_bar_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); 
	gtk_widget_set_size_request(tab_bar_container, -1, tab_bar_height + tab_spacer_height);
	
	GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_override_background_color(spacer, GTK_STATE_FLAG_NORMAL, &fg_color);
	gtk_widget_set_size_request(spacer, -1, tab_spacer_height);
	gtk_widget_show(spacer);
	
	gtk_box_pack_start(GTK_BOX(tab_bar_container), c->tab_bar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(tab_bar_container), spacer, FALSE, FALSE, 0);
	
	// Create a container to hold the black bar and the WebKitWebView
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	c->finder = webkit_web_view_get_find_controller(c->view);
	c->inspector = webkit_web_view_get_inspector(c->view);

	c->pageid = webkit_web_view_get_page_id(c->view);
	c->win = createwindow(c);

	// Add the black bar and the WebKitWebView to the container
	gtk_box_pack_start(GTK_BOX(vbox), tab_bar_container, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(c->view), TRUE, TRUE, 0);

	// Add the container to the main window
	gtk_container_add(GTK_CONTAINER(c->win), vbox);
	gtk_widget_show_all(c->win);
	gtk_widget_grab_focus(GTK_WIDGET(c->view));

	gwin = gtk_widget_get_window(GTK_WIDGET(c->win));
	c->xid = gdk_x11_window_get_xid(gwin);
	updatewinid(c);
	if (showxid) {
		gdk_display_sync(gtk_widget_get_display(c->win));
		puts(winid);
		fflush(stdout);
	}
	
	webkit_web_view_set_background_color(c->view, &bg_color);

	if (curconfig[HideBackground].val.i)
		webkit_web_view_set_background_color(c->view, &bg_color);

	if (!curconfig[KioskMode].val.i) {
		gdk_window_set_events(gwin, GDK_ALL_EVENTS_MASK);
		gdk_window_add_filter(gwin, processx, c);
	}

	if (curconfig[RunInFullscreen].val.i)
		togglefullscreen(c, NULL);

	if (curconfig[ZoomLevel].val.f != 1.0)
		webkit_web_view_set_zoom_level(c->view,
									   curconfig[ZoomLevel].val.f);

	setatom(c, AtomFind, "");
	setatom(c, AtomUri, "about:blank");
}

GtkWidget *
createwindow(Client *c)
{
	char *wmstr;
	GtkWidget *w;

	if (embed) {
		w = gtk_plug_new(embed);
	} else {
		w = gtk_window_new(GTK_WINDOW_TOPLEVEL);

		wmstr = g_path_get_basename(argv0);
		gtk_window_set_wmclass(GTK_WINDOW(w), wmstr, "Surf");
		g_free(wmstr);

		wmstr = g_strdup_printf("%s[%"PRIu64"]", "Surf", c->pageid);
		gtk_window_set_role(GTK_WINDOW(w), wmstr);
		g_free(wmstr);

		gtk_window_set_default_size(GTK_WINDOW(w), winsize[0], winsize[1]);
	}

	g_signal_connect(G_OBJECT(w), "destroy",
					 G_CALLBACK(destroywin), c);
	g_signal_connect(G_OBJECT(w), "enter-notify-event",
					 G_CALLBACK(winevent), c);
	g_signal_connect(G_OBJECT(w), "key-press-event",
					 G_CALLBACK(winevent), c);
	g_signal_connect(G_OBJECT(w), "leave-notify-event",
					 G_CALLBACK(winevent), c);
	g_signal_connect(G_OBJECT(w), "window-state-event",
					 G_CALLBACK(winevent), c);
	g_signal_connect(G_OBJECT(w), "button-press-event",
					 G_CALLBACK(tab_bar_mouse_press), c);
	g_signal_connect(G_OBJECT(w), "button-release-event",
					 G_CALLBACK(tab_bar_mouse_release), c);
	g_signal_connect(G_OBJECT(w), "motion-notify-event",
					 G_CALLBACK(tab_bar_mouse_move), c);

	return w;
}

gboolean
loadfailedtls(WebKitWebView *v, gchar *uri, GTlsCertificate *cert,
			  GTlsCertificateFlags err, Client *c)
{
	GString *errmsg = g_string_new(NULL);
	gchar *html, *pem;

	c->failedcert = g_object_ref(cert);
	c->tlserr = err;
	c->errorpage = 1;

	if (err & G_TLS_CERTIFICATE_UNKNOWN_CA)
		g_string_append(errmsg,
			"The signing certificate authority is not known.<br>");
	if (err & G_TLS_CERTIFICATE_BAD_IDENTITY)
		g_string_append(errmsg,
			"The certificate does not match the expected identity "
			"of the site that it was retrieved from.<br>");
	if (err & G_TLS_CERTIFICATE_NOT_ACTIVATED)
		g_string_append(errmsg,
			"The certificate's activation time "
			"is still in the future.<br>");
	if (err & G_TLS_CERTIFICATE_EXPIRED)
		g_string_append(errmsg, "The certificate has expired.<br>");
	if (err & G_TLS_CERTIFICATE_REVOKED)
		g_string_append(errmsg,
			"The certificate has been revoked according to "
			"the GTlsConnection's certificate revocation list.<br>");
	if (err & G_TLS_CERTIFICATE_INSECURE)
		g_string_append(errmsg,
			"The certificate's algorithm is considered insecure.<br>");
	if (err & G_TLS_CERTIFICATE_GENERIC_ERROR)
		g_string_append(errmsg,
			"Some error occurred validating the certificate.<br>");

	g_object_get(cert, "certificate-pem", &pem, NULL);
	html = g_strdup_printf("<p>Could not validate TLS for “%s”<br>%s</p>"
						   "<p>You can inspect the following certificate "
						   "with Ctrl-t (default keybinding).</p>"
						   "<p><pre>%s</pre></p>", uri, errmsg->str, pem);
	g_free(pem);
	g_string_free(errmsg, TRUE);

	webkit_web_view_load_alternate_html(c->view, html, uri, NULL);
	g_free(html);

	return TRUE;
}

void
loadchanged(WebKitWebView *v, WebKitLoadEvent e, Client *c)
{
	const char *uri = geturi(c);

	switch (e) {
	case WEBKIT_LOAD_STARTED:
		setatom(c, AtomUri, uri);
		c->title = uri;
		c->https = c->insecure = 0;
		seturiparameters(c, uri, loadtransient);
		if (c->errorpage)
			c->errorpage = 0;
		else
			g_clear_object(&c->failedcert);
		break;
	case WEBKIT_LOAD_REDIRECTED:
		setatom(c, AtomUri, uri);
		c->title = uri;
		seturiparameters(c, uri, loadtransient);
		break;
	case WEBKIT_LOAD_COMMITTED:
		setatom(c, AtomUri, uri);
		c->title = uri;
		seturiparameters(c, uri, loadcommitted);
		c->https = webkit_web_view_get_tls_info(c->view, &c->cert,
												&c->tlserr);
		break;
	case WEBKIT_LOAD_FINISHED:
		seturiparameters(c, uri, loadfinished);
		/* Disabled until we write some WebKitWebExtension for
		 * manipulating the DOM directly.
		evalscript(c, "document.documentElement.style.overflow = '%s'",
			enablescrollbars ? "auto" : "hidden");
		*/
		runscript(c);
		break;
	}
	updatetitle(c);
}

void
progresschanged(WebKitWebView *v, GParamSpec *ps, Client *c)
{
	c->progress = webkit_web_view_get_estimated_load_progress(c->view) *
				  100;
	updatetitle(c);
}

void
titlechanged(WebKitWebView *view, GParamSpec *ps, Client *c)
{
	c->title = webkit_web_view_get_title(c->view);
	updatetitle(c);
}

gboolean
viewusrmsgrcv(WebKitWebView *v, WebKitUserMessage *m, gpointer unused)
{
	WebKitUserMessage *r;
	GUnixFDList *gfd;
	const char *name;

	name = webkit_user_message_get_name(m);
	if (strcmp(name, "page-created") != 0) {
		fprintf(stderr, "surf: Unknown UserMessage: %s\n", name);
		return TRUE;
	}

	if (spair[1] < 0)
		return TRUE;

	gfd = g_unix_fd_list_new_from_array(&spair[1], 1);
	r = webkit_user_message_new_with_fd_list("surf-pipe", NULL, gfd);

	webkit_user_message_send_reply(m, r);

	return TRUE;
}

void
mousetargetchanged(WebKitWebView *v, WebKitHitTestResult *h, guint modifiers,
	Client *c)
{
	WebKitHitTestResultContext hc = webkit_hit_test_result_get_context(h);

	/* Keep the hit test to know where is the pointer on the next click */
	c->mousepos = h;

	if (hc & OnLink)
		c->targeturi = webkit_hit_test_result_get_link_uri(h);
	else if (hc & OnImg)
		c->targeturi = webkit_hit_test_result_get_image_uri(h);
	else if (hc & OnMedia)
		c->targeturi = webkit_hit_test_result_get_media_uri(h);
	else
		c->targeturi = NULL;

	c->overtitle = c->targeturi;
	updatetitle(c);
}

gboolean
permissionrequested(WebKitWebView *v, WebKitPermissionRequest *r, Client *c)
{
	ParamName param = ParameterLast;

	if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(r)) {
		param = Geolocation;
	} else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(r)) {
		if (webkit_user_media_permission_is_for_audio_device(
			WEBKIT_USER_MEDIA_PERMISSION_REQUEST(r)))
			param = AccessMicrophone;
		else if (webkit_user_media_permission_is_for_video_device(
				 WEBKIT_USER_MEDIA_PERMISSION_REQUEST(r)))
			param = AccessWebcam;
	} else {
		return FALSE;
	}

	if (curconfig[param].val.i)
		webkit_permission_request_allow(r);
	else
		webkit_permission_request_deny(r);

	return TRUE;
}

gboolean
decidepolicy(WebKitWebView *v, WebKitPolicyDecision *d,
	WebKitPolicyDecisionType dt, Client *c)
{
	switch (dt) {
	case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
		decidenavigation(d, c);
		break;
	case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
		decidenewwindow(d, c);
		break;
	case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
		decideresource(d, c);
		break;
	default:
		webkit_policy_decision_ignore(d);
		break;
	}
	return TRUE;
}

void
decidenavigation(WebKitPolicyDecision *d, Client *c)
{
	WebKitNavigationAction *a =
		webkit_navigation_policy_decision_get_navigation_action(
		WEBKIT_NAVIGATION_POLICY_DECISION(d));

	switch (webkit_navigation_action_get_navigation_type(a)) {
	case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_RELOAD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_OTHER: /* fallthrough */
	default:
		/* Do not navigate to links with a "_blank" target (popup) */
		if (webkit_navigation_policy_decision_get_frame_name(
			WEBKIT_NAVIGATION_POLICY_DECISION(d))) {
			webkit_policy_decision_ignore(d);
		} else {
			/* Filter out navigation to different domain ? */
			/* get action→urirequest, copy and load in new window+view
			 * on Ctrl+Click ? */
			webkit_policy_decision_use(d);
		}
		break;
	}
}

void
decidenewwindow(WebKitPolicyDecision *d, Client *c)
{
	Arg arg;
	WebKitNavigationAction *a =
		webkit_navigation_policy_decision_get_navigation_action(
		WEBKIT_NAVIGATION_POLICY_DECISION(d));


	switch (webkit_navigation_action_get_navigation_type(a)) {
	case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_RELOAD: /* fallthrough */
	case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
		/* Filter domains here */
/* If the value of “mouse-button” is not 0, then the navigation was triggered by a mouse event.
 * test for link clicked but no button ? */
		arg.v = webkit_uri_request_get_uri(
				webkit_navigation_action_get_request(a));
		newwindow(c, &arg, 0);
		break;
	case WEBKIT_NAVIGATION_TYPE_OTHER: /* fallthrough */
	default:
		break;
	}

	webkit_policy_decision_ignore(d);
}

void
decideresource(WebKitPolicyDecision *d, Client *c)
{
	int i, isascii = 1;
	WebKitResponsePolicyDecision *r = WEBKIT_RESPONSE_POLICY_DECISION(d);
	WebKitURIResponse *res =
		webkit_response_policy_decision_get_response(r);
	const gchar *uri = webkit_uri_response_get_uri(res);

	if (filter_request(uri)) {
		webkit_policy_decision_ignore(d);
		return;
	}

	if (!g_str_has_prefix(uri, "http://")
		&& !g_str_has_prefix(uri, "https://")
		&& !g_str_has_prefix(uri, "about:")
		&& !g_str_has_prefix(uri, "file://")
		&& !g_str_has_prefix(uri, "data:")
		&& !g_str_has_prefix(uri, "blob:")
		&& strlen(uri) > 0) {
		for (i = 0; i < strlen(uri); i++) {
			if (!g_ascii_isprint(uri[i])) {
				isascii = 0;
				break;
			}
		}
		if (isascii) {
			handleplumb(c, uri);
			webkit_policy_decision_ignore(d);
			return;
		}
	}

	if (webkit_response_policy_decision_is_mime_type_supported(r)) {
		webkit_policy_decision_use(d);
	} else {
		webkit_policy_decision_download(d);
	}
}

void
insecurecontent(WebKitWebView *v, WebKitInsecureContentEvent e, Client *c)
{
	c->insecure = 1;
}

void
webprocessterminated(WebKitWebView *v, WebKitWebProcessTerminationReason r,
					 Client *c)
{
	fprintf(stderr, "web process terminated: %s\n",
			r == WEBKIT_WEB_PROCESS_CRASHED ? "crashed" : "no memory");
	closeview(v, c);
}

void
closeview(WebKitWebView *v, Client *c)
{
	gtk_widget_destroy(c->win);
}

void
destroywin(GtkWidget* w, Client *c)
{
	destroyclient(c);
	if (!clients)
		gtk_main_quit();
}

gchar *
parseuri(const gchar *uri)
{
	guint i;

	for (i = 0; i < LENGTH(searchengines); i++) {
		if (g_str_has_prefix(uri, searchengines[i].token))
			return g_strdup_printf(searchengines[i].uri,
						   uri + strlen(searchengines[i].token));
	}

	return g_strdup_printf("http://%s", uri);
}

void
pasteuri(GtkClipboard *clipboard, const char *text, gpointer d)
{
	Arg a = {.v = text };
	if (text)
		loaduri((Client *) d, &a);
}

void
reload(Client *c, const Arg *a)
{
	if (a->i)
		webkit_web_view_reload_bypass_cache(c->view);
	else
		webkit_web_view_reload(c->view);
}

void
print(Client *c, const Arg *a)
{
	webkit_print_operation_run_dialog(webkit_print_operation_new(c->view),
									  GTK_WINDOW(c->win));
}

void
clipboard(Client *c, const Arg *a)
{
	if (a->i) { /* load clipboard uri */
		gtk_clipboard_request_text(gtk_clipboard_get(
								   GDK_SELECTION_PRIMARY),
								   pasteuri, c);
	} else { /* copy uri */
		gtk_clipboard_set_text(gtk_clipboard_get(
							   GDK_SELECTION_PRIMARY), c->targeturi
							   ? c->targeturi : geturi(c), -1);
	}
}

void
zoom(Client *c, const Arg *a)
{
	if (a->i > 0)
		webkit_web_view_set_zoom_level(c->view,
									   curconfig[ZoomLevel].val.f + 0.1);
	else if (a->i < 0)
		webkit_web_view_set_zoom_level(c->view,
									   curconfig[ZoomLevel].val.f - 0.1);
	else
		webkit_web_view_set_zoom_level(c->view, 1.0);

	curconfig[ZoomLevel].val.f = webkit_web_view_get_zoom_level(c->view);
}

static void
msgext(Client *c, char type, const Arg *a)
{
	static char msg[MSGBUFSZ];
	int ret;

	if (spair[0] < 0)
		return;

	if ((ret = snprintf(msg, sizeof(msg), "%lu%c%c", c->pageid, type, a->i))
		>= sizeof(msg)) {
		fprintf(stderr, "surf: message too long: %d\n", ret);
		return;
	}

	if (send(spair[0], msg, ret, 0) != ret)
		fprintf(stderr, "surf: error sending: %" PRIu64 "%c%d (%d)\n",
				c->pageid, type, a->i, ret);
}

void
scrollv(Client *c, const Arg *a)
{
	msgext(c, 'v', a);
}

void
scrollh(Client *c, const Arg *a)
{
	msgext(c, 'h', a);
}

void
navigate(Client *c, const Arg *a)
{
	if (a->i < 0)
		webkit_web_view_go_back(c->view);
	else if (a->i > 0)
		webkit_web_view_go_forward(c->view);
}

void
stop(Client *c, const Arg *a)
{
	webkit_web_view_stop_loading(c->view);
}

void
quit(Client *c, const Arg *a)
{
	cleanup();
	exit(0);
}

void
toggle(Client *c, const Arg *a)
{
	curconfig[a->i].val.i ^= 1;
	setparameter(c, 1, (ParamName)a->i, &curconfig[a->i].val);
}

void
togglefullscreen(Client *c, const Arg *a)
{
	/* toggling value is handled in winevent() */
	if (c->fullscreen)
		gtk_window_unfullscreen(GTK_WINDOW(c->win));
	else
		gtk_window_fullscreen(GTK_WINDOW(c->win));
}

void
togglecookiepolicy(Client *c, const Arg *a)
{
	++cookiepolicy;
	cookiepolicy %= strlen(curconfig[CookiePolicies].val.v);

	setparameter(c, 0, CookiePolicies, NULL);
}

void
toggleinspector(Client *c, const Arg *a)
{
	if (webkit_web_inspector_is_attached(c->inspector))
		webkit_web_inspector_close(c->inspector);
	else if (curconfig[Inspector].val.i)
		webkit_web_inspector_show(c->inspector);
}

void
find(Client *c, const Arg *a)
{
	const char *s, *f;

	if (a && a->i) {
		if (a->i > 0)
			webkit_find_controller_search_next(c->finder);
		else
			webkit_find_controller_search_previous(c->finder);
	} else {
		s = getatom(c, AtomFind);
		f = webkit_find_controller_get_search_text(c->finder);

		if (g_strcmp0(f, s) == 0) /* reset search */
			webkit_find_controller_search(c->finder, "", findopts,
										  G_MAXUINT);

		webkit_find_controller_search(c->finder, s, findopts,
									  G_MAXUINT);

		if (strcmp(s, "") == 0)
			webkit_find_controller_search_finish(c->finder);
	}
}

void
insert(Client *c, const Arg *a)
{
		insertmode = (a->i);
}

void
clicknavigate(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	navigate(c, a);
}

void
clicknewwindow(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	Arg arg;

	arg.v = webkit_hit_test_result_get_link_uri(h);
	newwindow(c, &arg, a->i);
}

void
clicknewtab(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	const gchar *uri = webkit_hit_test_result_get_link_uri(h);
	add_tab(c, uri);
}

void
clickexternplayer(Client *c, const Arg *a, WebKitHitTestResult *h)
{
	Arg arg;

	arg = (Arg)VIDEOPLAY(webkit_hit_test_result_get_link_uri(h));
	spawn(c, &arg);
}

void
playexternal(Client *c, const Arg *a)
{
	Arg arg;

	arg = (Arg)VIDEOPLAY(geturi(c));
	
	suspend_tab(c);
	
	spawn(c, &arg);
}

/* download-console */

void
downloadstarted(WebKitWebContext *wc, WebKitDownload *d, Client *c)
{
	webkit_download_set_allow_overwrite(d, TRUE);
	g_signal_connect(G_OBJECT(d), "decide-destination",
					 G_CALLBACK(decidedestination), NULL);
	g_signal_connect(G_OBJECT(d), "notify::estimated-progress",
					 G_CALLBACK(printprogress), NULL);
	g_signal_connect(G_OBJECT(d), "failed",
					 G_CALLBACK(downloadfailed), NULL);
	g_signal_connect(G_OBJECT(d), "finished",
					 G_CALLBACK(downloadfinished), NULL);
}

void
downloadfailed(WebKitDownload *d, GParamSpec *ps, void *arg)
{
	logdownload(d, " -- FAILED");
}

void
downloadfinished(WebKitDownload *d, GParamSpec *ps, void *arg)
{
	logdownload(d, " -- COMPLETED");
}

gboolean
decidedestination(WebKitDownload *d, gchar *suggested_filename, void *arg)
{
	gchar *dest;
	dest = g_strdup_printf("file://%s/%s", dldir, suggested_filename);
	webkit_download_set_destination(d, dest);
	return TRUE;
}

void
printprogress(WebKitDownload *d, GParamSpec *ps, void *arg)
{
	logdownload(d, "");
}

void
logdownload(WebKitDownload *d, gchar *tail)
{
	gchar *filename, *statfile;
	FILE *stat;

	filename = g_path_get_basename(webkit_download_get_destination(d));
	statfile = g_strdup_printf("%s/%s", dlstatus, filename);

	if ((stat = fopen(statfile, "w")) == NULL) {
		perror("dlstatus");
	} else {
		fprintf(stat, "%s: %d%% (%d.%ds)%s\n",
				filename,
				(int)(webkit_download_get_estimated_progress(d) * 100),
				(int) webkit_download_get_elapsed_time(d),
				(int)(webkit_download_get_elapsed_time(d) * 100),
				tail);
		fclose(stat);
	}

	g_free(statfile);
	g_free(filename);
}

void
spawndls(Client *c, const Arg *a)
{
	Arg arg = (Arg)DLSTATUS;
	spawn(c, &arg);
}

int
main(int argc, char *argv[])
{
	Arg arg;
	Client *c;

	memset(&arg, 0, sizeof(arg));

	/* command line args */
	ARGBEGIN {
	case 'a':
		defconfig[CookiePolicies].val.v = EARGF(usage());
		defconfig[CookiePolicies].prio = 2;
		break;
	case 'b':
		defconfig[ScrollBars].val.i = 0;
		defconfig[ScrollBars].prio = 2;
		break;
	case 'B':
		defconfig[ScrollBars].val.i = 1;
		defconfig[ScrollBars].prio = 2;
		break;
	case 'c':
		cookiefile = EARGF(usage());
		break;
	case 'C':
		stylefile = EARGF(usage());
		break;
	case 'd':
		defconfig[DiskCache].val.i = 0;
		defconfig[DiskCache].prio = 2;
		break;
	case 'D':
		defconfig[DiskCache].val.i = 1;
		defconfig[DiskCache].prio = 2;
		break;
	case 'e':
		embed = strtol(EARGF(usage()), NULL, 0);
		break;
	case 'f':
		defconfig[RunInFullscreen].val.i = 0;
		defconfig[RunInFullscreen].prio = 2;
		break;
	case 'F':
		defconfig[RunInFullscreen].val.i = 1;
		defconfig[RunInFullscreen].prio = 2;
		break;
	case 'g':
		defconfig[Geolocation].val.i = 0;
		defconfig[Geolocation].prio = 2;
		break;
	case 'G':
		defconfig[Geolocation].val.i = 1;
		defconfig[Geolocation].prio = 2;
		break;
	case 'i':
		defconfig[LoadImages].val.i = 0;
		defconfig[LoadImages].prio = 2;
		break;
	case 'I':
		defconfig[LoadImages].val.i = 1;
		defconfig[LoadImages].prio = 2;
		break;
	case 'k':
		defconfig[KioskMode].val.i = 0;
		defconfig[KioskMode].prio = 2;
		break;
	case 'K':
		defconfig[KioskMode].val.i = 1;
		defconfig[KioskMode].prio = 2;
		break;
	case 'm':
		defconfig[Style].val.i = 0;
		defconfig[Style].prio = 2;
		break;
	case 'M':
		defconfig[Style].val.i = 1;
		defconfig[Style].prio = 2;
		break;
	case 'n':
		defconfig[Inspector].val.i = 0;
		defconfig[Inspector].prio = 2;
		break;
	case 'N':
		defconfig[Inspector].val.i = 1;
		defconfig[Inspector].prio = 2;
		break;
	case 'r':
		scriptfile = EARGF(usage());
		break;
	case 's':
		defconfig[JavaScript].val.i = 0;
		defconfig[JavaScript].prio = 2;
		break;
	case 'S':
		defconfig[JavaScript].val.i = 1;
		defconfig[JavaScript].prio = 2;
		break;
	case 't':
		defconfig[StrictTLS].val.i = 0;
		defconfig[StrictTLS].prio = 2;
		break;
	case 'T':
		defconfig[StrictTLS].val.i = 1;
		defconfig[StrictTLS].prio = 2;
		break;
	case 'u':
		fulluseragent = EARGF(usage());
		break;
	case 'v':
		die("surf-"VERSION", see LICENSE for © details\n");
	case 'w':
		showxid = 1;
		break;
	case 'x':
		defconfig[Certificate].val.i = 0;
		defconfig[Certificate].prio = 2;
		break;
	case 'X':
		defconfig[Certificate].val.i = 1;
		defconfig[Certificate].prio = 2;
		break;
	case 'z':
		defconfig[ZoomLevel].val.f = strtof(EARGF(usage()), NULL);
		defconfig[ZoomLevel].prio = 2;
		break;
	default:
		usage();
	} ARGEND;
	if (argc > 0)
		arg.v = argv[0];
	else
		arg.v = "about:blank";

	setup();
	c = newclient(NULL);
	showview(NULL, c);

	loaduri(c, &arg);
	updatetitle(c);

	gtk_main();
	cleanup();

	return 0;
}
