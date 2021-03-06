/* modifier 0 means no modifier */
static int  surfuseragent   = 1;  /* Append Surf version to default WebKit user agent */
static char *fulluseragent  = ""; /* Or override the whole user agent string */
static char *scriptfile     = "~/.surf/script.js";
static char *styledir       = "~/.surf/styles/";
static char *certdir        = "~/.surf/certificates/";
static char *cachedir       = "~/.surf/cache/";
static char *cookiefile     = "~/.surf/cookies.txt";

#define BOOKMARKFILE "~/.surf/bookmarks"
#define HISTORYFILE  "~/.surf/history"

bool save_history = true;

/* Webkit default features */
/* Highest priority value will be used.
 * Default parameters are priority 0
 * Per-uri parameters are priority 1
 * Command parameters are priority 2
 */
static Parameter defconfig[ParameterLast] = {
	/* parameter                    Arg value       priority */
	[AccessMicrophone]    =       { { .i = 0 },     },
	[AccessWebcam]        =       { { .i = 0 },     },
	[Certificate]         =       { { .i = 0 },     },
	[CaretBrowsing]       =       { { .i = 0 },     },
	[ClipboardNotPrimary] =       { { .i = 0 },     },
	[CookiePolicies]      =       { { .v = "@Aa" }, },
	[DefaultCharset]      =       { { .v = "UTF-8" }, },
	[DiskCache]           =       { { .i = 1 },     },
	[DNSPrefetch]         =       { { .i = 0 },     },
	[Ephemeral]           =       { { .i = 0 },     },
	[FileURLsCrossAccess] =       { { .i = 0 },     },
	[FontSize]            =       { { .i = 16 },    },
	[FrameFlattening]     =       { { .i = 0 },     },
	[Geolocation]         =       { { .i = 1 },     },
	[HideBackground]      =       { { .i = 0 },     },
	[Inspector]           =       { { .i = 0 },     },
	[Java]                =       { { .i = 0 },     },
	[JavaScript]          =       { { .i = 1 },     },
	[KioskMode]           =       { { .i = 0 },     },
	[LoadImages]          =       { { .i = 1 },     },
	[MediaManualPlay]     =       { { .i = 1 },     },
	[PreferredLanguages]  =       { { .v = (char *[]){ NULL } }, },
	[RunInFullscreen]     =       { { .i = 0 },     },
	[ScrollBars]          =       { { .i = 1 },     },
	[ShowIndicators]      =       { { .i = 1 },     },
	[SiteQuirks]          =       { { .i = 1 },     },
	[SmoothScrolling]     =       { { .i = 0 },     },
	[SpellChecking]       =       { { .i = 0 },     },
	[SpellLanguages]      =       { { .v = ((char *[]){ "en_US", NULL }) }, },
	[StrictTLS]           =       { { .i = 1 },     },
	[Style]               =       { { .i = 0 },     },
	[WebGL]               =       { { .i = 0 },     },
	[ZoomLevel]           =       { { .f = 1.0 },   },
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
#define SETPROP_OPEN_URI(r, s, p) { \
        .v = (const char *[]){ "/bin/sh", "-c", \
             "prop=\"$(printf '%b' \"$(xprop -id $1 "r" " \
             "| sed -e 's/^"r"(UTF8_STRING) = \"\\(.*\\)\"/\\1/' " \
             "      -e 's/\\\\\\(.\\)/\\1/g' && cat "BOOKMARKFILE")\" " \
             "| dmenu -l 5 -p '"p"' -w $1)\" && " \
             "xprop -id $1 -f "s" 8u -set "s" \"$prop\"", \
             "surf-setprop", winid, NULL \
        } \
}

/* SETPROP(readprop, setprop, prompt)*/
#define SETPROP(r, s, p) { \
        .v = (const char *[]){ "/bin/sh", "-c", \
             "prop=\"$(printf '%b' \"$(xprop -id $1 "r" " \
             "| sed -e 's/^"r"(UTF8_STRING) = \"\\(.*\\)\"/\\1/' " \
             "      -e 's/\\\\\\(.\\)/\\1/g')\" " \
             "| dmenu -u -p '"p"' -w $1)\" && " \
             "xprop -id $1 -f "s" 8u -set "s" \"$prop\"", \
             "surf-setprop", winid, NULL \
        } \
}

/* DOWNLOAD(URI, referer) */
#define DOWNLOAD(u, r) { \
        .v = (const char *[]){ "st", "-e", "/bin/sh", "-c",\
             "curl -g -L -J --output-dir ${XDG_DOWNLOAD_DIR:-$HOME/downloads}/surf -O" \
             " -A \"$1\" -b \"$2\" -c \"$2\" -e \"$3\" \"$4\"; read", \
             "surf-download", useragent, cookiefile, r, u, NULL \
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

#define WATCH { \
    .v = (char *[]){ "/bin/sh", "-c", \
    	"st -e \
    	yt $(xprop -id $0 _SURF_URI | cut -d \\\" -f 2)", \
	winid, NULL } \
}

/* BOOKMARK_ADD(r) */
#define BOOKMARK_ADD(r) {\
        .v = (const char *[]){ "/bin/sh", "-c", \
             "uri=\"$(echo $(xprop -id $0 $1) | cut -d '\"' -f2 | sed 's/.*https*:\\/\\/\\(www\\.\\)\\?//' | dmenu -e -u -p \"Add to bookmarks:\" -w $0)\"" \
             " && (echo \"$uri\" && cat "BOOKMARKFILE") | awk '!seen[$0]++' > "BOOKMARKFILE".tmp" \
             " && mv "BOOKMARKFILE".tmp "BOOKMARKFILE, \
             winid, r, NULL \
        } \
}

/* HISTORYFILE_ADD(r) */
#define HISTORY_ADD(r) { \
        .v = (const char *[]){ "/bin/sh", "-c", \
             "uri=\"$(echo $0 | cut -d '\"' -f2 | sed 's/.*https*:\\/\\/\\(www\\.\\)\\?//')\"" \
             " && hist_line=\"$(echo $(date +'(%Y-%m-%d %H:%M:%S)') $uri)\"" \
             " && [ \"$(awk 'NR==1 {print $NF}' "HISTORYFILE")\" != \"$uri\" ]" \
             " && sed -i \"1i $hist_line\" "HISTORYFILE, \
             r, NULL \
        } \
}

/* HISTORY_NAV*/
#define HISTORY_NAV { \
        .v = (const char *[]){ "/bin/sh", "-c", \
             "hist_line=\"$(cat "HISTORYFILE" | dmenu -l 50 -p 'Goto history:' -w $1)\"" \
             " && grep \"^$hist_line\$\" "HISTORYFILE ">/dev/null" \
             " && uri=\"$(sed 's|^(.*) \\(.*\\)$|\\1|' <<<$hist_line)\"" \
             " && xprop -id $1 -f _SURF_GO 8u -set _SURF_GO \"$uri\"", \
             "surf-setprop", winid, NULL \
        } \
}

/* BOOKMARK_REMOVE_DMENU */
#define BOOKMARK_REMOVE_DMENU { \
        .v = (const char *[]){ "/bin/sh", "-c", \
             "line=\"$(cat "BOOKMARKFILE" | dmenu -l 5 -n -p 'Delete bookmark:' -w $0)\"" \
             " && sed -i $((line++))d "BOOKMARKFILE, winid, NULL \
        } \
}

/* BOOKMARK_REMOVE_URI(r) */
#define BOOKMARK_REMOVE_URI(r) { \
        .v = (const char *[]){ "/bin/bash", "-c", \
        	"choice=\"$(echo 'n\ny' | dmenu -p 'Remove current uri from bookmarks? [yN]:' -w $0)\"; [[ \"$choice\" =~ [yY] ]]" \
        	" && uri=\"$(xprop -id $0 $1 | cut -d '\"' -f 2 | sed 's/.*https*:\\/\\/\\(www\\.\\)\\?//')\" " \
            " && sed -i \"\\|$uri|d\" "BOOKMARKFILE, winid, r, NULL \
        } \
}

/* styles */
/*
 * The iteration will stop at the first match, beginning at the beginning of
 * the list.
 */
static SiteSpecific styles[] = {
	/* regexp               file in $styledir */
	{ ".*",                 "default.css" },
};

/* certificates */
/*
 * Provide custom certificate for urls
 */
static SiteSpecific certs[] = {
	/* regexp               file in $certdir */
	{ "://suckless\\.org/", "suckless.org.crt" },
};

// #define MODKEY GDK_CONTROL_MASK
#define GDK_ALT_MASK 8
#define MODKEY GDK_ALT_MASK

/* hotkeys */
/*
 * If you use anything else but MODKEY and GDK_SHIFT_MASK, don't forget to
 * edit the CLEANMASK() macro.
 */
void pass() {}
#define HOME "https://search.bus-hit.me"
static char *searchengine = "https://search.bus-hit.me/?q=";
static Key keys[] = {
	/* modifier              keyval          function    arg */
	{ 0,                     GDK_KEY_o,      spawn,      SETPROP_OPEN_URI("_SURF_URI", "_SURF_GO", PROMPT_GO) },
	{ 0,                     GDK_KEY_slash,  spawn,      SETPROP("_SURF_FIND", "_SURF_FIND", PROMPT_FIND) },

	{ 0,                     GDK_KEY_i,      insert,     { .i = 1 } },
	{ 0,                     GDK_KEY_Escape, insert,     { .i = 0 } },

	{ 0,                     GDK_KEY_Escape, stop,       { 0 } },
	{ 0,                     GDK_KEY_c,      stop,       { 0 } },

	{ 0|GDK_SHIFT_MASK,      GDK_KEY_r,      reload,     { .i = 1 } },
	{ 0,                     GDK_KEY_r,      reload,     { .i = 0 } },

	{ 0|GDK_SHIFT_MASK,      GDK_KEY_l,      navigate,   { .i = +1 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_h,      navigate,   { .i = -1 } },

	/* vertical and horizontal scrolling, in viewport percentage */
	{ 0,                     GDK_KEY_j,      scrollv,    { .i = +10 } },
	{ 0,                     GDK_KEY_k,      scrollv,    { .i = -10 } },
	{ 0,                     GDK_KEY_d,      scrollv,    { .i = +50 } },
	{ 0,                     GDK_KEY_u,      scrollv,    { .i = -50 } },
	{ 0,                     GDK_KEY_l,      scrollh,    { .i = +10 } },
	{ 0,                     GDK_KEY_h,      scrollh,    { .i = -10 } },

	// { MODKEY,                GDK_KEY_b,      loaduri,   { .v = HOME } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_b,      spawn,      BOOKMARK_ADD("_SURF_URI") },
	{ MODKEY,                GDK_KEY_d,      spawn,      BOOKMARK_REMOVE_DMENU },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_d,      spawn,      BOOKMARK_REMOVE_URI("_SURF_URI") },

	{ MODKEY,                GDK_KEY_m,      spawn,      HISTORY_NAV },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_u,      download_current_uri, { 0 } },

    { MODKEY|GDK_SHIFT_MASK, GDK_KEY_v,      spawn,      WATCH },


	{ 0|GDK_SHIFT_MASK,      GDK_KEY_j,      zoom,       { .i = -1 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_k,      zoom,       { .i = +1 } },
	{ 0,                     GDK_KEY_minus,  zoom,       { .i = -1 } },
	{ 0,                     GDK_KEY_plus,   zoom,       { .i = +1 } },

	{ MODKEY,                GDK_KEY_p,      clipboard,  { .i = 1 } },
	{ MODKEY,                GDK_KEY_y,      clipboard,  { .i = 0 } },


	{ 0,                     GDK_KEY_n,      find,       { .i = +1 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_n,      find,       { .i = -1 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_p,      print,      { 0 } },
	{ MODKEY,                GDK_KEY_z,      showcert,   { 0 } },

	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_a,      togglecookiepolicy, { 0 } },
	{ 0,                     GDK_KEY_F11,    togglefullscreen, { 0 } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_o,      toggleinspector, { 0 } },

	// { MODKEY|GDK_SHIFT_MASK, GDK_KEY_c,      toggle,     { .i = CaretBrowsing } },
	// { MODKEY|GDK_SHIFT_MASK, GDK_KEY_f,      toggle,     { .i = FrameFlattening } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_g,      toggle,     { .i = Geolocation } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_s,      toggle,     { .i = JavaScript } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_i,      toggle,     { .i = LoadImages } },
	// { MODKEY|GDK_SHIFT_MASK, GDK_KEY_b,      toggle,     { .i = ScrollBars } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_t,      toggle,     { .i = StrictTLS } },
	{ MODKEY|GDK_SHIFT_MASK, GDK_KEY_m,      toggle,     { .i = Style } },

	// the modal patch makes it so that no keys that are used in a mapping (without MODKEY)
	// can be typed while in 'normal' mode. so we add all of these mappings for unused keys
	// to a pass(), which does nothing, so that they're not accidentally typed while in normal
	// mode
	{ 0,                     GDK_KEY_a,      pass,       { 0 } },
	{ 0,                     GDK_KEY_b,      pass,       { 0 } },
	{ 0,                     GDK_KEY_e,      pass,       { 0 } },
	{ 0,                     GDK_KEY_f,      pass,       { 0 } },
	{ 0,                     GDK_KEY_g,      pass,       { 0 } },
	{ 0,                     GDK_KEY_m,      pass,       { 0 } },
	{ 0,                     GDK_KEY_p,      pass,       { 0 } },
	{ 0,                     GDK_KEY_q,      pass,       { 0 } },
	{ 0,                     GDK_KEY_s,      pass,       { 0 } },
	{ 0,                     GDK_KEY_t,      pass,       { 0 } },
	{ 0,                     GDK_KEY_v,      pass,       { 0 } },
	{ 0,                     GDK_KEY_w,      pass,       { 0 } },
	{ 0,                     GDK_KEY_x,      pass,       { 0 } },
	{ 0,                     GDK_KEY_y,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_a,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_b,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_c,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_d,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_e,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_g,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_i,      pass,       { 0 } },
	// { 0|GDK_SHIFT_MASK,      GDK_KEY_m,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_o,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_p,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_q,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_s,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_t,      pass,       { 0 } },
	// { 0|GDK_SHIFT_MASK,      GDK_KEY_u,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_v,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_w,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_x,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_y,      pass,       { 0 } },
	{ 0|GDK_SHIFT_MASK,      GDK_KEY_z,      pass,       { 0 } },

};

/* button definitions */
/* target can be OnDoc, OnLink, OnImg, OnMedia, OnEdit, OnBar, OnSel, OnAny */
static Button buttons[] = {
	/* target       event mask      button  function        argument        stop event */
	{ OnLink,       0,              2,      clicknewwindow, { .i = 0 },     1 },
	{ OnLink,       MODKEY,         2,      clicknewwindow, { .i = 1 },     1 },
	{ OnLink,       MODKEY,         1,      clicknewwindow, { .i = 1 },     1 },
	{ OnAny,        0,              8,      clicknavigate,  { .i = -1 },    1 },
	{ OnAny,        0,              9,      clicknavigate,  { .i = +1 },    1 },
	{ OnMedia,      MODKEY,         1,      clickexternplayer, { 0 },       1 },
};
