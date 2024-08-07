/* modifier 0 means no modifier */
static int surfuseragent	= 1;  /* Append Surf version to default WebKit user agent */
static char *fulluseragent  = ""; /* Or override the whole user agent string */
static char *scriptfile	 = "~/.config/savoia/script.js";
static char *styledir	   = "~/.config/savoia/styles/";
static char *certdir		= "~/.config/savoia/certificates/";
static char *dlstatus	   = "~/.config/savoia/dlstatus/";
static char *cachedir	   = "/tmp/cache";
static char *cookiefile	 = "/tmp/cookies.txt";
static char *dldir		  = "~/dl/";
	
static int tab_bar_height = 27;
static int tab_spacer_height = 4;
static const char *tab_bar_color[] = {"#222222", "#318d56", "#444444"};
static int min_tab_fraction_size = 4; // 1/4th of the screen

static SearchEngine searchengines[] = {
	{ " ", "https://web.penwing.org/search?q=%s" },
	{ "-", "https://searx.ox2.fr/search?q=%s" },
};

/* Regular expressions to match URLs that should not be loaded */
char *filter_patterns[] = {
	#include "filters_compiled"
};
/* Define this for verbose filtering */
// #define FILTER_VERBOSE


/* Webkit default features */
/* Highest priority value will be used.
 * Default parameters are priority 0
 * Per-uri parameters are priority 1
 * Command parameters are priority 2
 */
static Parameter defconfig[ParameterLast] = {
	/* parameter					Arg value	   priority */
	[AccessMicrophone]	=	   { { .i = 0 },	 },
	[AccessWebcam]		=	   { { .i = 0 },	 },
	[Certificate]		 =	   { { .i = 0 },	 },
	[CaretBrowsing]	   =	   { { .i = 0 },	 },
	[CookiePolicies]	  =	   { { .v = "@Aa" }, },
	[DarkMode]			=	   { { .i = 0 },	 },
	[DefaultCharset]	  =	   { { .v = "UTF-8" }, },
	[DiskCache]		   =	   { { .i = 1 },	 },
	[DNSPrefetch]		 =	   { { .i = 1 },	 },
	[Ephemeral]		   =	   { { .i = 0 },	 },
	[FileURLsCrossAccess] =	   { { .i = 0 },	 },
	[FontSize]			=	   { { .i = 12 },	},
	[FrameFlattening]	 =	   { { .i = 1 },	 },
	[Geolocation]		 =	   { { .i = 0 },	 },
	[HideBackground]	  =	   { { .i = 0 },	 },
	[Inspector]		   =	   { { .i = 0 },	 },
	[Java]				=	   { { .i = 0 },	 },
	[JavaScript]		  =	   { { .i = 1 },	 },
	[KioskMode]		   =	   { { .i = 0 },	 },
	[LoadImages]		  =	   { { .i = 0 },	 },
	[MediaManualPlay]	 =	   { { .i = 1 },	 },
	[PreferredLanguages]  =	   { { .v = (char *[]){ NULL } }, },
	[RunInFullscreen]	 =	   { { .i = 0 },	 },
	[ScrollBars]		  =	   { { .i = 1 },	 },
	[ShowIndicators]	  =	   { { .i = 1 },	 },
	[SiteQuirks]		  =	   { { .i = 1 },	 },
	[SmoothScrolling]	 =	   { { .i = 0 },	 },
	[SpellChecking]	   =	   { { .i = 0 },	 },
	[SpellLanguages]	  =	   { { .v = ((char *[]){ "en_US", NULL }) }, },
	[StrictTLS]		   =	   { { .i = 1 },	 },
	[Style]			   =	   { { .i = 1 },	 },
	[WebGL]			   =	   { { .i = 0 },	 },
	[ZoomLevel]		   =	   { { .f = 1.1 },   },
};

static UriParameters uriparams[] = {
	{ "(://|\\.)suckless\\.org(/|$)", {
	  [JavaScript] = { { .i = 0 }, 1 },
	}, },
};

/* default window size: width, height */
static int winsize[] = { 800, 600 };

static WebKitFindOptions findopts = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE |
									WEBKIT_FIND_OPTIONS_WRAP_AROUND;

#define PROMPT_GO   "Go:"
#define PROMPT_FIND "Find:"

/* SETPROP(readprop, setprop, prompt)*/
#define SETPROP(r, s, p) { \
	.v = (const char *[]){ "/bin/sh", "-c", \
		 "prop=\"$(printf '%b' \"$(xprop -id $1 "r" " \
		 "| sed -e 's/^"r"(UTF8_STRING) = \"\\(.*\\)\"/\\1/' " \
		 "	  -e 's/\\\\\\(.\\)/\\1/g' && cat ~/.config/savoia/bookmarks && echo 'keybinds')\" " \
		 "| marukuru -p '"p"' -w $1)\" " \
		 "&& xprop -id $1 -f "s" 8u -set "s" \"$prop\"", \
		 "surf-setprop", winid, NULL \
	} \
}

#define DLSTATUS { \
	.v = (const char *[]){ "kodama", "-e", "/bin/sh", "-c",\
		"while true; do cat $1/* 2>/dev/null || echo \"nothing to download\";"\
		"A=; read A; "\
		"if [ $A = \"clean\" ]; then rm $1/*; fi; clear; done",\
		"surf-dlstatus", dlstatus, NULL \
	} \
}

/* PLUMB(URI) */
/* This called when some URI which does not begin with "about:",
 * "http://" or "https://" should be opened.
 */
#define PLUMB(u) {\
	.v = (const char *[]){ "/bin/sh", "-c", \
		 "xdg-open \"$0\"", u, NULL \
	} \
}

/* VIDEOPLAY(URI) */
#define VIDEOPLAY(u) {\
	.v = (const char *[]){ "/bin/sh", "-c", \
		 "mpv --really-quiet \"$0\"", u, NULL \
	} \
}

/* BM_ADD(readprop) */
#define BM_ADD(r) {\
	.v = (const char *[]){ "/bin/sh", "-c", \
		"(echo $(xprop -id $0 $1) | cut -d '\"' -f2 " \
		"| sed 's/.*https*:\\/\\/\\(www\\.\\)\\?//' && cat ~/.config/savoia/bookmarks) " \
		"| awk '!seen[$0]++' > ~/.config/savoia/bookmarks.tmp && " \
		"mv ~/.config/savoia/bookmarks.tmp ~/.config/savoia/bookmarks &&" \
		"notify-send -u normal -a 'savoia' 'added bookmark'", \
		winid, r, NULL \
	} \
}

/* styles */
/*
 * The iteration will stop at the first match, beginning at the beginning of
 * the list.
 */
static SiteSpecific styles[] = {
	/* regexp			   file in $styledir */
	{ ".*",				 "default.css" },
};

/* certificates */
/*
 * Provide custom certificate for urls
 */
static SiteSpecific certs[] = {
	/* regexp			   file in $certdir */
	{ "://suckless\\.org/", "suckless.org.crt" },
};

#define MODKEY GDK_CONTROL_MASK

/* hotkeys */
/*
 * If you use anything else but MODKEY and GDK_SHIFT_MASK, dont forget to
 * edit the CLEANMASK() macro.
 */
static Key keys[] = {
	/* modifier			  keyval		  function	arg */
	{ MODKEY,				GDK_KEY_Return, spawn,	  SETPROP("_SURF_URI", "_SURF_GO", PROMPT_GO) },
	{ MODKEY,				GDK_KEY_f,	  spawn,	  SETPROP("_SURF_FIND", "_SURF_FIND", PROMPT_FIND) },
	{ MODKEY,				GDK_KEY_slash,  spawn,	  SETPROP("_SURF_FIND", "_SURF_FIND", PROMPT_FIND) },
	{ MODKEY,				GDK_KEY_b,	  spawn,	  BM_ADD("_SURF_URI") },
	{ MODKEY,				GDK_KEY_w,	  playexternal, { 0 } },
	{ MODKEY,				GDK_KEY_d,	  spawndls,   { 0 } },
	
	{ MODKEY,				GDK_KEY_Left,   switch_tab, { .i = -1 } },
	{ MODKEY,				GDK_KEY_Right,  switch_tab, { .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_Left,   move_tab,	{ .i = -1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_Right,  move_tab,	{ .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_Down,   close_tab,   { .i = -1 } },
	{ MODKEY,				GDK_KEY_Up, new_tab,	{ 0 } },
	
	{ MODKEY,				GDK_KEY_r,	  reload,	 { .i = 0 } },

	{ MODKEY,				GDK_KEY_i,	  insert,	 { .i = 1 } },
	{ MODKEY,				GDK_KEY_Escape, insert,	 { .i = 0 } },	

	/* vertical and horizontal scrolling, in viewport percentage */
	// { MODKEY,				GDK_KEY_k,	  scrollv,	{ .i = +30 } },
	// { MODKEY,				GDK_KEY_j,	  scrollv,	{ .i = -30 } },
	// { MODKEY,				GDK_KEY_l,	  scrollh,	{ .i = +30 } },
	// { MODKEY,				GDK_KEY_h,	  scrollh,	{ .i = -30 } },

	{ MODKEY, 			   	GDK_KEY_equal,  zoom,	   { .i = 0  } },
	{ MODKEY,				GDK_KEY_F4,  zoom,	   { .i = -1 } },
	{ MODKEY,				GDK_KEY_F5,   zoom,	   { .i = +1 } },

	{ MODKEY, 				GDK_KEY_p,	  clipboard,  { .i = 1 } },
	{ MODKEY, 				GDK_KEY_y,	  clipboard,  { .i = 0 } },

	{ MODKEY,				GDK_KEY_space,	  find,	   { .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_space,	  find,	   { .i = -1 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_k,	  togglecookiepolicy, { 0 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_i,	  toggleinspector, { 0 } },

	{ MODKEY, 				GDK_KEY_F6,	  toggle,	 { .i = Geolocation } },
	{ MODKEY, 				GDK_KEY_F7,	  toggle,	 { .i = CaretBrowsing } },
	{ MODKEY, 				GDK_KEY_F8,	  toggle,	 { .i = DarkMode } },
	{ MODKEY, 				GDK_KEY_F9,	  toggle,	 { .i = StrictTLS } },
	{ MODKEY, 				GDK_KEY_F10,	  toggle,	 { .i = LoadImages } },
	{ MODKEY, 				GDK_KEY_F11,	  toggle,	 { .i = FrameFlattening } },
	{ MODKEY, 				GDK_KEY_F12,	  toggle,	 { .i = JavaScript } },
	//{ MODKEY,				GDK_KEY_l,	  navigate,   { .i = +1 } },
	//{ MODKEY,				GDK_KEY_h,	  navigate,   { .i = -1 } },
	//{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_b,	  toggle,	 { .i = ScrollBars } },
	//{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_m,	  toggle,	 { .i = Style } },
};

/* button definitions */
/* target can be OnDoc, OnLink, OnImg, OnMedia, OnEdit, OnBar, OnSel, OnAny */
static Button buttons[] = {
	/* target		event mask		button  	function			argument		stop event */
	{ OnLink,		0,				2,			clicknewtab,		{ .i = 0 },	 	1 },
	{ OnLink,		MODKEY,			1,			clicknewtab,		{ .i = 0 },	 	1 },
	{ OnLink,		MODKEY,			2,			clicknewwindow,		{ .i = 0 },	 	1 },
	{ OnLink,		GDK_SHIFT_MASK,	1,			clickexternplayer,	{ 0 },	   		1 },
	{ OnAny,		0,				8,			clicknavigate,		{ .i = -1 },	1 },
	{ OnAny,		0,				9,			clicknavigate,		{ .i = +1 },	1 },
};


const char *suspended_html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title></title><style>body{background-color:#222;color:#444;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;font-size:220px;}</style></head><body>⏾</body></html>";

const char *keybinds_url = "keybinds";
const char *keybinds_html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Keybinds</title>"
"<style>body {background-color: #222;color: #aaa;height: 100vh;margin: 80px;font-size: 20px;}table {width: 100%;border-collapse: collapse;}th, td {padding: 10px;border: 1px solid #444;}th {background-color: #333;}td {background-color: #2a2a2a;}.section-header {background-color: #444;color: #fff;text-align: left;font-weight: bold;}</style></head>"
"<body><h2>Keybinds, all with Ctrl modifier</h2><table><tr><th>Keys</th><th>Action</th></tr><tr>"
"<td colspan=\"2\" class=\"section-header\">Utilities</td></tr><tr><td>return</td><td>go to url</td></tr><tr><td>b</td><td>bookmark page</td></tr><tr><td>d</td><td>download manager</td></tr><tr><td>shift + i</td><td>inspector</td></tr><tr>"
"<td colspan=\"2\" class=\"section-header\">Tab Management</td></tr><tr><td>⇆ </td><td>switch tabs</td></tr><tr><td>shift + ⇆ </td><td>move tabs</td></tr><tr><td>shift + ⬇ </td><td>close tab</td></tr><tr><td>⬆ </td><td>new tab</td></tr><tr><td>r</td><td>reload tab</td></tr><tr>"
"<td colspan=\"2\" class=\"section-header\">Moving</td></tr><tr><td>f</td><td>find prompt</td></tr><tr><td>space</td><td>find next</td></tr><tr><td>shift + space</td><td>find previous</td></tr><tr><td>equal</td><td>reset zoom</td></tr><tr><td>F4</td><td>decrease zoom</td></tr><tr><td>F5</td><td>increase zoom</td></tr><tr>"
"<td colspan=\"2\" class=\"section-header\">Toggles</td></tr><tr><td>F6</td><td>geolocation</td></tr><tr><td>F8</td><td>dark mode</td></tr><tr><td>F9</td><td>strict tls</td></tr><tr><td>F10</td><td>load images</td></tr><tr><td>F11</td><td>frame flattening</td></tr><tr><td>F12</td><td>javascript</td></tr></table>"
"<h2>Mousebinds</h2><table><tr><th>Trigger</th><th>Action</th></tr><tr>"
"<td colspan=\"2\" class=\"section-header\">On links</td></tr><tr><td>middle click</td><td>open in new tab</td></tr><tr><td>ctrl + left click</td><td>open in new tab</td></tr><tr><td>ctrl + middle click</td><td>open in new window</td></tr><tr><td>shift + left click</td><td>open in mpv</td></tr></table></body></html>";
