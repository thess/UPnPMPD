/* output_mpd.c - Output module for MPD client routines
 *
 * Copyright (C) 2012	     Ted Hess (Kitschensync)
 *
 * This file is part of UPnPMPD.
 *
 * UPnPMPD is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UPnPMPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UPnPMPD; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <libconfig.h>
#include <upnp/ithread.h>
#include <mpd/client.h>

#include "logging.h"
#include "upnp_connmgr.h"
#include "upnp_control.h"
#include "upnp_transport.h"
#include "output_mpd.h"

// Net timeout in seconds
#define MPD_TIMEOUT 5

static int mpdvolume = 0;
static int mutevolume = 0;
static int track_duration;
static char tempbuf[32];
static struct mpd_connection *mpd_conn = NULL;

static gchar *options_host = NULL;
static gint options_port = 0;
static gchar *options_password = NULL;
static gboolean test_mode = FALSE;

#define HOST_DEFAULT    "localhost"
#define PORT_DEFAULT    6600

static ithread_mutex_t mpd_mutex = PTHREAD_MUTEX_INITIALIZER;

DBG_STATIC void update_mpd_status(void);

typedef enum {
    STATUS_TEST = -1,
    STATUS_OK = 0,
    STATUS_FAIL = 1
} CNX_STATUS;

// MIME types list (really?)
static const char *mpd_mime_types[] = {
    "audio/mpeg",
    "audio/mp4",
    "audio/m4a",
    "audio/mp2",
    "audio/mpc",
    "audio/ogg",
    "audio/AMR",
    "audio/AMR-WB",
    "audio/amr-nb-sh",
    "audio/amr-wb-sh",
    "audio/3gpp",
    "audio/adpcm",
    "audio/L16",
    "audio/ape",
    "audio/flac",
    "audio/wav",
    "audio/x-ape",
    "audio/x-flac",
    "audio/x-ogg",
    "audio/x-wave",
    "audio/x-wav",
    "audio/x-wavpack",
    "audio/x-ms-wma",
    "audio/x-ms-wmv",
    "audio/x-wma",
    NULL
};

static void output_printError(const char *tag)
{
	const char *message;

	message = mpd_connection_get_error_message(mpd_conn);
    // messages received from the server are UTF-8; the
    //   rest is either US-ASCII or locale */
	//if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
    //    message = charset_from_utf8(message);

	fprintf(stderr, "error: (%s) returned %s\n", tag, message);
	// Close and free MPD connection
	mpd_connection_free(mpd_conn);
	mpd_conn = NULL;

	return;
}

DBG_STATIC struct mpd_connection *setup_connection(void)
{
	mpd_conn = mpd_connection_new(options_host, options_port, MPD_TIMEOUT * 1000);
	if (mpd_conn == NULL) {
		fputs("MPD connection - no memory!\n", stderr);
		return NULL;
	}

	if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS)
	{
	    printf("Failed to connect %s:%d\n", options_host, options_port);
		output_printError(__FUNCTION__);
		return NULL;
	}

	if(options_password)
	{
		if (!mpd_run_password(mpd_conn, options_password))
            output_printError(__FUNCTION__);
	}

	return mpd_conn;
}

// Attempt (re-)connection
DBG_STATIC CNX_STATUS check_mpd_connection(bool update_status)
{
    int rc = STATUS_FAIL;

    // If test mode - no connection to MPD
    if (test_mode)
        return STATUS_TEST;

	ithread_mutex_lock(&mpd_mutex);

    if (!mpd_conn)
    {
        mpd_conn = setup_connection();
        if (mpd_conn)
        {
            // Maybe reconnect - update status
            if (update_status)
                update_mpd_status();
            // Success
            rc = STATUS_OK;
        }
    } else
        rc = STATUS_OK;

	ithread_mutex_unlock(&mpd_mutex);

    return rc;
}

// Convert UPnP time HH:MM:SS
// *** TODO: Need to handle relative time correctly !!
DBG_STATIC int parsetimetosecs(const char *value)
{
    int hrs, mins, secs;
    char *nptr;

    // May be NULL
    if (!value)
        return 0;

    hrs = strtol(value, &nptr, 10);
    nptr++;
    mins = strtol(nptr, &nptr, 10);
    nptr++;
    secs = strtol(nptr, &nptr, 10);

    return hrs * 3600 + mins * 60 + secs;
}

void output_set_uri(const char *uri)
{
	DBG_PRINT(DBG_LVL1, "%s: setting MPD uri to '%s'\n", __FUNCTION__, uri);

    if (mpd_conn == NULL)
        return;

	ithread_mutex_lock(&mpd_mutex);

    // Clear existing playlist (we want this to be the only entry)
    mpd_run_clear(mpd_conn);
    // Reflect updated (STOPPED) player status

    // URI is already UTF8
    if (!mpd_run_add(mpd_conn, uri))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "Error adding %s: %s\n", uri, message);
            return;
		}
		output_printError(__FUNCTION__);
    }

    ithread_mutex_unlock(&mpd_mutex);

	return;
}

int output_play(void)
{
    // Check MPD connection
    if (check_mpd_connection(FALSE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_play(mpd_conn))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error playing stream - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    update_mpd_status();

    ithread_mutex_unlock(&mpd_mutex);

	return 0;
}

int output_stop(void)
{
    // Check MPD connection
    if (check_mpd_connection(FALSE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_stop(mpd_conn))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error stopping stream - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    update_mpd_status();

    ithread_mutex_unlock(&mpd_mutex);

	return 0;
}

int output_pause(void)
{
    // Check MPD connection
    if (check_mpd_connection(FALSE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_pause(mpd_conn, true))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error pausing stream - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    update_mpd_status();

    ithread_mutex_unlock(&mpd_mutex);

	return 0;
}

int output_next(void)
{
    // Check MPD connection
    if (check_mpd_connection(FALSE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_next(mpd_conn))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error setting next stream - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    update_mpd_status();

    ithread_mutex_unlock(&mpd_mutex);

   return 0;
}

int output_prev(void)
{
    // Check MPD connection
    if (check_mpd_connection(FALSE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_previous(mpd_conn))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error setting previous stream - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    update_mpd_status();

    ithread_mutex_unlock(&mpd_mutex);

    return 0;
}

int output_seekto(const char *seekmode, const char *seekpos)
{
    struct mpd_status *mstatus;
    int sid, seekto;

    // Check MPD connection
    if (check_mpd_connection(TRUE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    mstatus = mpd_run_status(mpd_conn);
    if (mstatus == NULL)
    {
        ithread_mutex_unlock(&mpd_mutex);
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error getting status - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    if (mpd_status_get_state(mstatus) == MPD_STATE_STOP)
    {
        DBG_PRINT(DBG_LVL1, "Player stopped -- cannot seek\n");
        mpd_status_free(mstatus);

        ithread_mutex_unlock(&mpd_mutex);
        return -1;
    }

    // Get correct song-id
    sid = mpd_status_get_song_id(mstatus);

    // Convert HH:MM:SS to seconds
    seekto = parsetimetosecs(seekpos);

    //if (strcmp(seekmode, "REL_TIME") == 0)
    //    seekto = mpd_status_get_elapsed_time(mstatus) + seekto;

    if (seekto < 0) seekto = 0;
    if (seekto > track_duration)
        seekto = track_duration;

    DBG_PRINT(DBG_LVL4, "Seeking to: %d in %d from %d\n", seekto, track_duration, mpd_status_get_elapsed_time(mstatus));

    mpd_status_free(mstatus);

    if (!mpd_run_seek_id(mpd_conn, sid, seekto))
    {
        ithread_mutex_unlock(&mpd_mutex);
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error seeking stream - %s\n", message);
            return -1;
		}
		output_printError(__FUNCTION__);
		return -1;
    }

    ithread_mutex_unlock(&mpd_mutex);

    return 0;
}

int output_playmode(const char *newmode)
{
    int rc = 0;
    // Check MPD connection
    if (check_mpd_connection(TRUE) != STATUS_OK)
        return (test_mode) ? 0 : -1;

    ithread_mutex_lock(&mpd_mutex);

    if (strcmp(newmode, "NORMAL") == 0)
    {
        if (!mpd_command_list_begin(mpd_conn, FALSE) ||
            !mpd_send_single(mpd_conn, FALSE) ||
            !mpd_send_random(mpd_conn, FALSE) ||
            !mpd_send_repeat(mpd_conn, FALSE) ||
            !mpd_command_list_end(mpd_conn))
        {
            mpd_response_finish(mpd_conn);
            output_printError(__FUNCTION__);
            rc = -1;
            goto out;
        }

    } else if (strcmp(newmode, "REPEAT-ONE") == 0)
    {
        if (!mpd_command_list_begin(mpd_conn, FALSE) ||
            !mpd_send_single(mpd_conn, TRUE) ||
            !mpd_send_random(mpd_conn, FALSE) ||
            !mpd_send_repeat(mpd_conn, TRUE) ||
            !mpd_command_list_end(mpd_conn))
        {
            mpd_response_finish(mpd_conn);
            output_printError(__FUNCTION__);
            rc = -1;
            goto out;
        }
    } else if (strcmp(newmode, "DIRECT_1") == 0)
    {
        if (!mpd_command_list_begin(mpd_conn, FALSE) ||
            !mpd_send_single(mpd_conn, TRUE) ||
            !mpd_send_random(mpd_conn, FALSE) ||
            !mpd_send_repeat(mpd_conn, FALSE) ||
            !mpd_command_list_end(mpd_conn))
        {
            mpd_response_finish(mpd_conn);
            output_printError(__FUNCTION__);
            rc = -1;
            goto out;
        }
    } else if (strcmp(newmode, "REPEAT-ALL") == 0)
    {
        if (!mpd_command_list_begin(mpd_conn, FALSE) ||
            !mpd_send_single(mpd_conn, FALSE) ||
            !mpd_send_random(mpd_conn, FALSE) ||
            !mpd_send_repeat(mpd_conn, TRUE) ||
            !mpd_command_list_end(mpd_conn))
        {
            mpd_response_finish(mpd_conn);
            output_printError(__FUNCTION__);
            rc = -1;
            goto out;
        }
    } else if (strcmp(newmode, "RANDOM") == 0)
    {
        if (!mpd_command_list_begin(mpd_conn, FALSE) ||
            !mpd_send_single(mpd_conn, FALSE) ||
            !mpd_send_random(mpd_conn, TRUE) ||
            !mpd_send_repeat(mpd_conn, FALSE) ||
            !mpd_command_list_end(mpd_conn))
        {
            mpd_response_finish(mpd_conn);
            output_printError(__FUNCTION__);
            rc = -1;
            goto out;
        }
    } else
        rc = -1;

out:
    // Cleanup if success
    if (rc == 0)
    {
        if (!mpd_response_finish(mpd_conn))
            output_printError(__FUNCTION__);
    }

    ithread_mutex_unlock(&mpd_mutex);

    return rc;
}

void output_set_volume(const char *newvol)
{
    int val;

    // Check MPD connection
    if (check_mpd_connection(TRUE) != STATUS_OK)
        return;

    // Validate input request (0-100)
    if (!newvol)
        return;

    val = atoi(newvol);
    if (val < 0)
        val = 0;
    if (val > 100)
        val = 100;

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_set_volume(mpd_conn, val))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error setting volume - %s\n", message);
            return;
		}
		output_printError(__FUNCTION__);
    }

    // Keep local copy of volume
    mpdvolume = val;
    mutevolume = val;

    ithread_mutex_unlock(&mpd_mutex);

    return;
}

void output_set_mute(bool bmute)
{
    int newvolume;

    // Check MPD connection
    if (check_mpd_connection(TRUE) != STATUS_OK)
        return;

    if (bmute)
    {
        // Save current volume for restore
        mutevolume = mpdvolume;
        newvolume = 0;
    } else {
        // Restore volume
        newvolume = mutevolume;
    }

    ithread_mutex_lock(&mpd_mutex);

    if (!mpd_run_set_volume(mpd_conn, newvolume))
    {
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            ithread_mutex_unlock(&mpd_mutex);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error setting volume - %s\n", message);
            return;
		}
		output_printError(__FUNCTION__);
    }

    mpdvolume = newvolume;

    ithread_mutex_unlock(&mpd_mutex);

    return;
}

const char *output_get_volume(void)
{
    snprintf(tempbuf, 6, "%d", mpdvolume);

    return (const char *)tempbuf;
}

/*************************************************************
 *  DIDL-LITE metadata short-form template for MPD ID3 tags  *
 *************************************************************/

#define DIDL_LITE_TEMPLATE  \
    "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\"" \
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\"" \
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"" \
        "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">" \
            "<item id=\"xyzzy$1$0\" parentID=\"xyzzy$1\" restricted=\"1\">" \
                "<upnp:class>object.item.audioItem.musicTrack</upnp:class>" \
                "<dc:title>%s</dc:title>" \
                "<upnp:album>%s</upnp:album>" \
                "<upnp:artist>%s</upnp:artist>" \
                "<upnp:originalTrackNumber>%s</upnp:originalTrackNumber>" \
                "<dc:date>%s</dc:date>" \
                "<res protocolInfo=\"http-get:*:audio/mpeg:DLNA.ORG_PN=MP3\" duration=\"%s\"> %s</res>" \
            "</item>" \
    "</DIDL-Lite>"

DBG_STATIC const char *get_song_tag(struct mpd_song *song, const char *tag)
{
    const char *sval;
    enum mpd_tag_type tag_type;

    // Find ID3 tag values from song data
    tag_type = mpd_tag_name_iparse(tag);
    if (tag_type != MPD_TAG_UNKNOWN)
    {
        sval = mpd_song_get_tag(song, tag_type, 0);
        if (sval)
        {
            DBG_PRINT(DBG_LVL4, "ID3 tag found %s = %s\n", tag, sval);
            return sval;
        }
    }
    // Not found
    return NULL;
}

DBG_STATIC void get_track_metadata(struct mpd_song *song)
{
    char *sval;
    const char *mtitle, *malbum, *martist, *mtrack, *mdate, *mname;

    // Get current meta data
    sval = transport_get_var(TRANSPORT_VAR_AV_URI_META);
    if (!sval || (strcmp(sval, "") == 0))
    {
        // Empty metadata - Do we also have a URI
        sval = transport_get_var(TRANSPORT_VAR_AV_URI);
        if (sval && (strcmp(sval, "") != 0))
        {
            // We have URI - make up some metadata
            mtitle = get_song_tag(song, "title");
            malbum = get_song_tag(song, "album");
            martist = get_song_tag(song, "artist");
            mtrack = get_song_tag(song, "track");
            mdate = get_song_tag(song, "date");
            mname = get_song_tag(song, "name");

            // Must have a title or name
            if (mtitle || mname)
            {
                // Default "name" (if needed)
                if (!mname)
                    mname = "Unknown";

                // Use 'name' if no title or album
                if (!mtitle)
                    mtitle = mname;
                else if (!malbum)
                    malbum = mname;

                // Fill in some defaults
                if (!malbum)
                    malbum = "Unknown";
                if (!martist)
                    martist = "Unknown";
                if (!mtrack)
                    mtrack = "1";
                if (!mdate)
                    mdate = "1980-01-01T00:00:00";

                // Convert ID3 tags to UPnP DCS <DIDL-Lite\>
                asprintf(&sval,
                         DIDL_LITE_TEMPLATE,
                         mtitle, malbum, martist, mtrack, mdate,
                         transport_get_var(TRANSPORT_VAR_CUR_TRACK_DUR),
                         transport_get_var(TRANSPORT_VAR_AV_URI));

                // Set track and transport metadata
                transport_set_var(TRANSPORT_VAR_CUR_TRACK_META, sval);
                transport_set_var(TRANSPORT_VAR_AV_URI_META, sval);

                free(sval);
            }
        }
    }

    return;
}

// Note: caller must hold transport_mutex
DBG_STATIC void update_track_position(struct mpd_status *mstatus)
{
    char buf[16];
    int val;
    int hh, mm, ss;

    // Track # (position in queue)
    val = mpd_status_get_song_pos(mstatus);
    snprintf(buf, 6, "%d", val + 1);
    transport_set_var(TRANSPORT_VAR_CUR_TRACK, buf);

    // play position (relative)
    val = mpd_status_get_elapsed_time(mstatus);
    snprintf(buf, 10, "%d", val);
    transport_set_var(TRANSPORT_VAR_REL_CTR_POS, buf);
    transport_set_var(TRANSPORT_VAR_ABS_CTR_POS, buf);

    // Convert to hh:mm:ss
    hh = val / 3600;
    mm = (val - (hh * 3600)) / 60;
    ss = val - (hh * 3600) - (mm * 60);
    snprintf(buf, 10, "%02d:%02d:%02d", hh, mm, ss);
    transport_set_var(TRANSPORT_VAR_REL_TIME_POS, buf);
    transport_set_var(TRANSPORT_VAR_ABS_TIME_POS, buf);

    return;
}

// Note: caller must hold mpd_mutext
DBG_STATIC void update_mpd_status(void)
{
    char buf[16];
    struct mpd_status *mstatus;
    struct mpd_song *song;
    const char *sval;
    int hh, mm, ss;

    // Check MPD connection
    if (!mpd_conn)
        return;


    if (!mpd_command_list_begin(mpd_conn, true) ||
        !mpd_send_status(mpd_conn) ||
        !mpd_send_list_queue_meta(mpd_conn) ||
        !mpd_command_list_end(mpd_conn))
    {
        output_printError(__FUNCTION__);
        return;
    }

    mstatus = mpd_recv_status(mpd_conn);
    if (mstatus == NULL) {
        mpd_response_finish(mpd_conn);
        output_printError(__FUNCTION__);
        return;
    }

    if (!mpd_response_next(mpd_conn))
    {
        mpd_response_finish(mpd_conn);
        output_printError(__FUNCTION__);
        mpd_status_free(mstatus);
        return;
    }

    song = mpd_recv_song(mpd_conn);
    if (song != NULL)
    {
        // Song duration
        track_duration = mpd_song_get_duration(song);
        if (track_duration == 0)
            track_duration = mpd_status_get_total_time(mstatus);
        // Anything?
        if (track_duration == 0)
        {
            // See if we have the URI metadate (last resort)
            track_duration = parsetimetosecs(transport_get_var(TRANSPORT_VAR_CUR_TRACK_DUR));
        } else {
            // Convert track duration to hh:mm:ss
            hh = track_duration / 3600;
            mm = (track_duration - (hh * 3600)) / 60;
            ss = track_duration - (hh * 3600) - (mm * 60);
            snprintf(buf, 10, "%02d:%02d:%02d", hh, mm, ss);
            transport_set_var(TRANSPORT_VAR_CUR_TRACK_DUR, buf);
        }

        // Get current meta data
        sval = transport_get_var(TRANSPORT_VAR_AV_URI);
        if (!sval || (strcmp(sval, "") == 0))
        {
            sval = mpd_song_get_uri(song);
            if (sval)
            {
                // Set the current URI for track and transport
                transport_set_var(TRANSPORT_VAR_CUR_TRACK_URI, (char *)sval);
                transport_set_var(TRANSPORT_VAR_AV_URI, (char *)sval);

                get_track_metadata(song);
            }
        }

        // Release song data
        mpd_song_free(song);
    }

    if (!mpd_response_finish(mpd_conn))
        output_printError(__FUNCTION__);

    // Current volume setting
    mpdvolume = mpd_status_get_volume(mstatus);

    switch(mpd_status_get_state(mstatus))
    {
    case MPD_STATE_UNKNOWN:
        // no information available
        fprintf(stderr, "MPD state unknown\n");
        break;

    case MPD_STATE_STOP:
        // not playing
        transport_set_state(TRANSPORT_STOPPED, "STOPPED");
        break;

    case MPD_STATE_PLAY:
        // playing
        transport_set_state(TRANSPORT_PLAYING, "PLAYING");
        break;

    case MPD_STATE_PAUSE:
        // playing, but paused
        transport_set_state(TRANSPORT_PAUSED_PLAYBACK, "PAUSED_PLAYBACK");
        break;
    }

    update_track_position(mstatus);

    mpd_status_free(mstatus);

    return;
}

void output_update_status(void)
{
	ithread_mutex_lock(&mpd_mutex);

    update_mpd_status();

	ithread_mutex_unlock(&mpd_mutex);

    return;
}

void output_update_position(void)
{
    struct mpd_status *mstatus;

    // Check MPD connection
    if (check_mpd_connection(FALSE) != STATUS_OK)
        return;

	ithread_mutex_lock(&mpd_mutex);

    mstatus = mpd_run_status(mpd_conn);
    if (mstatus == NULL)
    {
        ithread_mutex_unlock(&mpd_mutex);
		if (mpd_connection_get_error(mpd_conn) == MPD_ERROR_SERVER)
		{
            /* we've got an error message from the server */
            const char *message = mpd_connection_get_error_message(mpd_conn);
            //message = charset_from_utf8(message);
            fprintf(stderr, "error getting status - %s\n", message);
            return;
		}
		output_printError(__FUNCTION__);
		return;
    }

    update_track_position(mstatus);

    mpd_status_free(mstatus);

	ithread_mutex_unlock(&mpd_mutex);

    return;
}

int output_loop()
{
	GMainLoop *loop;

    // Get current state before servicing requests
    output_update_status();

	/* Create a main loop that runs the default GLib main context */
	loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(loop);

	return 0;
}



/* Options specific to output_mpd */
static GOptionEntry option_entries[] = {
        { "host", 'h', 0, G_OPTION_ARG_STRING, &options_host,
          "MPD host name or IP ", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &options_port,
          "MPD host port number ", NULL },
        { "password", 'P', 0, G_OPTION_ARG_STRING, &options_password,
          "MPD host password ", NULL },
        { "testmode", 't', 0, G_OPTION_ARG_NONE, &test_mode,
           "testmode - OK if no MPD", NULL },
        { NULL }
};


int output_mpd_add_options(GOptionContext *ctx)
{
	GOptionGroup *option_group;

	option_group = g_option_group_new("mpdout", "MPD Output Options",
	                                  "Show MPD Output Options",
	                                  NULL, NULL);
	g_option_group_add_entries(option_group, option_entries);

	g_option_context_add_group(ctx, option_group);

	return 0;
}

int output_mpd_init(config_t *cfg)
{
    const char **ptype = &mpd_mime_types[0];

    // Register mime types
    while (*ptype)
    {
        register_mime_type(*ptype);
        ptype++;
    }

    // Setup config file defaults
    // Determine host and port
    if (options_host == NULL)
    {
        // Check config
        if (config_lookup_string(cfg, "host", (const char **)&options_host) != CONFIG_TRUE)
        {
            options_host = HOST_DEFAULT;
        }
    }

    if (options_port == 0)
    {
        if (config_lookup_int(cfg, "port", (long *)&options_port) != CONFIG_TRUE)
        {
            options_port = PORT_DEFAULT;
        }
    }

    if (options_password == NULL)
        config_lookup_string(cfg, "password", (const char **)&options_password);


    if (check_mpd_connection(FALSE) != STATUS_OK)
        return (test_mode) ? 0 : 1;

    return 0;
}

/********* Sample DIDL-Lite ***************
'<DIDL-Lite xmlns="urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/"
    xmlns:upnp="urn:schemas-upnp-org:metadata-1-0/upnp/"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:dlna="urn:schemas-dlna-org:metadata-1-0/">
        <item id="taz1l0z1l3z1l1547zal290" parentID="taz1l0z1l3z1l1547z5l1548l40" restricted="1">
            <upnp:class>object.item.audioItem.musicTrack</upnp:class>
            <dc:title>Kubik</dc:title>
            <dc:creator>Perry O'Neil</dc:creator>
            <upnp:artist>Perry O'Neil</upnp:artist>
            <upnp:albumArtURI>http://10.0.2.35:53168/content/wacky-file-name</upnp:albumArtURI>
            <upnp:genre>Electronica &amp; Dance</upnp:genre>
            <upnp:album>State of Trance 2004 Disc 2</upnp:album>
            <upnp:originalTrackNumber>1</upnp:originalTrackNumber>
            <dc:date>2004-01-01T00:00:00</dc:date>
            <res protocolInfo="http-get:*:audio/mpeg:DLNA.ORG_PN=MP3;DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000" bitrate="16000" sampleFrequency="44100" duration="0:07:47.000">
                    http://10.0.2.35:53168/content/taz1l0z1l3z1l1547zal290</res>
        </item>
</DIDL-Lite>
******************************************/
