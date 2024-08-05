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
	[DNSPrefetch]		 =	   { { .i = 0 },	 },
	[Ephemeral]		   =	   { { .i = 0 },	 },
	[FileURLsCrossAccess] =	   { { .i = 0 },	 },
	[FontSize]			=	   { { .i = 12 },	},
	[FrameFlattening]	 =	   { { .i = 0 },	 },
	[Geolocation]		 =	   { { .i = 0 },	 },
	[HideBackground]	  =	   { { .i = 0 },	 },
	[Inspector]		   =	   { { .i = 0 },	 },
	[Java]				=	   { { .i = 1 },	 },
	[JavaScript]		  =	   { { .i = 1 },	 },
	[KioskMode]		   =	   { { .i = 0 },	 },
	[LoadImages]		  =	   { { .i = 1 },	 },
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
	[ZoomLevel]		   =	   { { .f = 1.0 },   },
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
		 "	  -e 's/\\\\\\(.\\)/\\1/g' && cat ~/.config/savoia/bookmarks)\" " \
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
			"notify-send -u low -a 'savoia' 'added bookmark'", \
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
	{ MODKEY,				GDK_KEY_g,	  spawn,	  SETPROP("_SURF_URI", "_SURF_GO", PROMPT_GO) },
	{ MODKEY,				GDK_KEY_f,	  spawn,	  SETPROP("_SURF_FIND", "_SURF_FIND", PROMPT_FIND) },
	{ MODKEY,				GDK_KEY_slash,  spawn,	  SETPROP("_SURF_FIND", "_SURF_FIND", PROMPT_FIND) },
	{ MODKEY,				GDK_KEY_b,	  spawn,	  BM_ADD("_SURF_URI") },
	{ MODKEY,				GDK_KEY_w,	  playexternal, { 0 } },
	{ MODKEY,				GDK_KEY_d,	  spawndls,   { 0 } },

	{ MODKEY,				GDK_KEY_i,	  insert,	 { .i = 1 } },
	{ MODKEY,				GDK_KEY_Escape, insert,	 { .i = 0 } },	

	{ MODKEY,				GDK_KEY_r,	  reload,	 { .i = 0 } },

	{ MODKEY,				GDK_KEY_l,	  navigate,   { .i = +1 } },
	{ MODKEY,				GDK_KEY_h,	  navigate,   { .i = -1 } },

	/* vertical and horizontal scrolling, in viewport percentage */
	{ MODKEY,				GDK_KEY_j,	  scrollv,	{ .i = +10 } },
	{ MODKEY,				GDK_KEY_k,	  scrollv,	{ .i = -10 } },
	{ MODKEY,				GDK_KEY_o,	  scrollh,	{ .i = +10 } },
	{ MODKEY,				GDK_KEY_u,	  scrollh,	{ .i = -10 } },

	{ MODKEY, 			   GDK_KEY_equal,  zoom,	   { .i = 0  } },
	{ MODKEY,				GDK_KEY_minus,  zoom,	   { .i = -1 } },
	{ MODKEY,				GDK_KEY_plus,   zoom,	   { .i = +1 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_v,	  clipboard,  { .i = 1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_c,	  clipboard,  { .i = 0 } },

	{ MODKEY,				GDK_KEY_n,	  find,	   { .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_n,	  find,	   { .i = -1 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_a,	  togglecookiepolicy, { 0 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_o,	  toggleinspector, { 0 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_b,	  toggle,	 { .i = CaretBrowsing } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_f,	  toggle,	 { .i = FrameFlattening } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_g,	  toggle,	 { .i = Geolocation } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_s,	  toggle,	 { .i = JavaScript } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_i,	  toggle,	 { .i = LoadImages } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_b,	  toggle,	 { .i = ScrollBars } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_t,	  toggle,	 { .i = StrictTLS } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_m,	  toggle,	 { .i = Style } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_d,	  toggle,	 { .i = DarkMode } },
	{ MODKEY,				GDK_KEY_Left,   switch_tab, { .i = -1 } },
	{ MODKEY,				GDK_KEY_Right,  switch_tab, { .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_Left,   move_tab,	{ .i = -1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_Right,  move_tab,	{ .i = +1 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_Down,   close_tab,   { .i = -1 } },
	{ MODKEY,				GDK_KEY_Up, new_tab,	{ 0 } },
};

/* button definitions */
/* target can be OnDoc, OnLink, OnImg, OnMedia, OnEdit, OnBar, OnSel, OnAny */
static Button buttons[] = {
	/* target	   event mask	  button  function		argument		stop event */
	{ OnLink,	   0,		 	 2,	  clicknewtab,	{ .i = 0 },	 1 },
	{ OnLink,	   MODKEY,		 1,	  clicknewtab,	{ .i = 0 },	 1 },
	{ OnLink,	   MODKEY,		 2,	  clicknewwindow, { .i = 0 },	 1 },
	{ OnAny,		0,			  8,	  clicknavigate,  { .i = -1 },	1 },
	{ OnAny,		0,			  9,	  clicknavigate,  { .i = +1 },	1 },
	{ OnMedia,	  MODKEY,		 1,	  clickexternplayer, { 0 },	   1 },
};
