#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

#include <mpd/client.h>

static Display *dpy;
static struct mpd_connection* mpdconn = NULL;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char*
getMPDState()
{
	struct mpd_status* stat = NULL;
	struct mpd_song* song = NULL;
	const char* title = NULL;
	const char* artist = NULL;
	char* ret = NULL;

	if(!mpdconn)
		return smprintf("");

	mpd_command_list_begin(mpdconn, true);
	mpd_send_status(mpdconn);
	mpd_send_current_song(mpdconn);
	mpd_command_list_end(mpdconn);

	stat = mpd_recv_status(mpdconn);
	if(stat && (mpd_status_get_state(stat) == MPD_STATE_PLAY)) {
		//display info
		mpd_response_next(mpdconn);
		song = mpd_recv_song(mpdconn);
		artist = smprintf("%s", mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));
		title = smprintf("%s", mpd_song_get_tag(song, MPD_TAG_TITLE, 0));
		ret = smprintf("%s - %s |", artist, title);
		// cleanup
		mpd_song_free(song);
		free((char*)artist);
		free((char*)title);
	} else {
		//nothing
		ret = smprintf("");;
	}
	mpd_status_free(stat);
	mpd_response_finish(mpdconn);

	return ret;
}

int
main(void)
{
	char* status = NULL;
	char* tmlcl = NULL;
	char* mpdstr = NULL;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	// open MPD connection
	mpdconn = mpd_connection_new("localhost", 0, 0);
	if(!mpdconn || mpd_connection_get_error(mpdconn)) {
		fprintf(stderr, "dwmstatus: %s\n", mpd_connection_get_error_message(mpdconn));
		return 1;
	}

	for (;;sleep(1)) {
		tmlcl = mktimes("%a, %d. %b, %H:%M", "Europe/Berlin");
		mpdstr = getMPDState();
		status = smprintf(" %s %s ", mpdstr, tmlcl);

		setstatus(status);
		free(tmlcl);
		free(mpdstr);
		free(status);
	}

	mpd_connection_free(mpdconn);
	XCloseDisplay(dpy);

	return 0;
}

