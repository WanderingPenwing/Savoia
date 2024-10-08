#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <webkit2/webkit-web-extension.h>
#include <webkitdom/webkitdom.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

#include "common.h"

#define LENGTH(x)   (sizeof(x) / sizeof(x[0]))

static WebKitWebExtension *webext;
static int sock;

static void
msgsurf(guint64 pageid, const char *s)
{
	static char msg[MSGBUFSZ];
	size_t sln = strlen(s);
	int ret;

	if ((ret = snprintf(msg, sizeof(msg), "%" PRIu64 "%s", pageid, s))
		>= sizeof(msg)) {
		fprintf(stderr, "webext: msg: message too long: %d\n", ret);
		return;
	}

	if (send(sock, msg, ret, 0) < 0)
		fprintf(stderr, "webext: error sending: %s\n", msg+1);
}

static gboolean
readsock(GIOChannel *s, GIOCondition c, gpointer unused)
{
	static char js[48], msg[MSGBUFSZ];
	WebKitWebPage *page;
	JSCContext *jsc;
	GError *gerr = NULL;
	gsize msgsz;

	if (g_io_channel_read_chars(s, msg, sizeof(msg), &msgsz, &gerr) !=
		G_IO_STATUS_NORMAL) {
		if (gerr) {
			fprintf(stderr, "webext: error reading socket: %s\n",
					gerr->message);
			g_error_free(gerr);
		}
		return TRUE;
	}

	if (msgsz < 2) {
		fprintf(stderr, "webext: readsock: message too short: %zu\n",
				msgsz);
		return TRUE;
	}

	if (!(page = webkit_web_extension_get_page(webext, msg[0])))
		return TRUE;

	jsc = webkit_frame_get_js_context(webkit_web_page_get_main_frame(page));

	switch (msg[1]) {
	case 'h':
		if (msgsz != 3)
			return TRUE;
		snprintf(js, sizeof(js),
				 "window.scrollBy(window.innerWidth/100*%d,0);",
				 msg[2]);
		JSCValue *h_result = jsc_context_evaluate(jsc, js, -1);
		if (h_result) {
        	free(h_result);
	    } else {
	        fprintf(stderr, "Error evaluating JavaScript\n");
	    }
		break;
	case 'v':
		if (msgsz != 3)
			return TRUE;
		snprintf(js, sizeof(js),
				 "window.scrollBy(0,window.innerHeight/100*%d);",
				 msg[2]);
		JSCValue *v_result = jsc_context_evaluate(jsc, js, -1);
		if (v_result) {
        	free(v_result);
	    } else {
	        fprintf(stderr, "Error evaluating JavaScript\n");
	    }
		break;
	}

	return TRUE;
}

static void
pageusermessagereply(GObject *o, GAsyncResult *r, gpointer page)
{
	WebKitUserMessage *m;
	GUnixFDList *gfd;
	GIOChannel *gchansock;
	const char *name;
	int nfd;

	m = webkit_web_page_send_message_to_view_finish(page, r, NULL);
	name = webkit_user_message_get_name(m);
	if (strcmp(name, "surf-pipe") != 0) {
		fprintf(stderr, "webext-surf: Unknown User Reply: %s\n", name);
		return;
	}

	gfd = webkit_user_message_get_fd_list(m);
	if ((nfd = g_unix_fd_list_get_length(gfd)) != 1) {
		fprintf(stderr, "webext-surf: Too many file-descriptors: %d\n", nfd);
		return;
	}

	sock = g_unix_fd_list_get(gfd, 0, NULL);

	gchansock = g_io_channel_unix_new(sock);
	g_io_channel_set_encoding(gchansock, NULL, NULL);
	g_io_channel_set_flags(gchansock, g_io_channel_get_flags(gchansock)
						   | G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_close_on_unref(gchansock, TRUE);
	g_io_add_watch(gchansock, G_IO_IN, readsock, NULL);
}

void
pagecreated(WebKitWebExtension *e, WebKitWebPage *p, gpointer unused)
{
	WebKitUserMessage *msg;

	msg = webkit_user_message_new("page-created", NULL);
	webkit_web_page_send_message_to_view(p, msg, NULL, pageusermessagereply, p);
}

G_MODULE_EXPORT void
webkit_web_extension_initialize(WebKitWebExtension *e)
{
	webext = e;

	g_signal_connect(G_OBJECT(e), "page-created",
					 G_CALLBACK(pagecreated), NULL);
}
