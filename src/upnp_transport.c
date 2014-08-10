/* upnp_transport.c - UPnP AVTransport routines
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
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

#include <upnp/upnp.h>
#include <upnp/ithread.h>
#include <upnp/upnptools.h>

#include "logging.h"

#include "xmlescape.h"
#include "upnp.h"
#include "upnp_device.h"
#include "upnp_control.h"
#include "upnp_transport.h"
#include "output_mpd.h"

#define TRANSPORT_SERVICE "urn:schemas-upnp-org:service:AVTransport"
#define TRANSPORT_TYPE "urn:schemas-upnp-org:service:AVTransport:1"
#define TRANSPORT_SCPD_URL "/upnp/rendertransportSCPD.xml"
#define TRANSPORT_CONTROL_URL "/upnp/control/rendertransport1"
#define TRANSPORT_EVENT_URL "/upnp/event/rendertransport1"

static const char *transport_variables[] =
{
	[TRANSPORT_VAR_TRANSPORT_STATE] = "TransportState",
	[TRANSPORT_VAR_TRANSPORT_STATUS] = "TransportStatus",
	[TRANSPORT_VAR_PLAY_MEDIUM] = "PlaybackStorageMedium",
	[TRANSPORT_VAR_REC_MEDIUM] = "RecordStorageMedium",
	[TRANSPORT_VAR_PLAY_MEDIA] = "PossiblePlaybackStorageMedia",
	[TRANSPORT_VAR_REC_MEDIA] = "PossibleRecordStorageMedia",
	[TRANSPORT_VAR_CUR_PLAY_MODE] = "CurrentPlayMode",
	[TRANSPORT_VAR_TRANSPORT_PLAY_SPEED] = "TransportPlaySpeed",
	[TRANSPORT_VAR_REC_MEDIUM_WR_STATUS] = "RecordMediumWriteStatus",
	[TRANSPORT_VAR_CUR_REC_QUAL_MODE] = "CurrentRecordQualityMode",
	[TRANSPORT_VAR_POS_REC_QUAL_MODE] = "PossibleRecordQualityModes",
	[TRANSPORT_VAR_NR_TRACKS] = "NumberOfTracks",
	[TRANSPORT_VAR_CUR_TRACK] = "CurrentTrack",
	[TRANSPORT_VAR_CUR_TRACK_DUR] = "CurrentTrackDuration",
	[TRANSPORT_VAR_CUR_MEDIA_DUR] = "CurrentMediaDuration",
	[TRANSPORT_VAR_CUR_TRACK_META] = "CurrentTrackMetaData",
	[TRANSPORT_VAR_CUR_TRACK_URI] = "CurrentTrackURI",
	[TRANSPORT_VAR_AV_URI] = "AVTransportURI",
	[TRANSPORT_VAR_AV_URI_META] = "AVTransportURIMetaData",
	[TRANSPORT_VAR_NEXT_AV_URI] = "NextAVTransportURI",
	[TRANSPORT_VAR_NEXT_AV_URI_META] = "NextAVTransportURIMetaData",
	[TRANSPORT_VAR_REL_TIME_POS] = "RelativeTimePosition",
	[TRANSPORT_VAR_ABS_TIME_POS] = "AbsoluteTimePosition",
	[TRANSPORT_VAR_REL_CTR_POS] = "RelativeCounterPosition",
	[TRANSPORT_VAR_ABS_CTR_POS] = "AbsoluteCounterPosition",
	[TRANSPORT_VAR_LAST_CHANGE] = "LastChange",
	[TRANSPORT_VAR_AAT_SEEK_MODE] = "A_ARG_TYPE_SeekMode",
	[TRANSPORT_VAR_AAT_SEEK_TARGET] = "A_ARG_TYPE_SeekTarget",
	[TRANSPORT_VAR_AAT_INSTANCE_ID] = "A_ARG_TYPE_InstanceID",
	[TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS] = "CurrentTransportActions",	/* optional */
	[TRANSPORT_VAR_UNKNOWN] = NULL
};

static const char *transport_defaults[] =
{
	[TRANSPORT_VAR_TRANSPORT_STATE] = "STOPPED",
	[TRANSPORT_VAR_TRANSPORT_STATUS] = "OK",
	[TRANSPORT_VAR_PLAY_MEDIUM] = "UNKNOWN",
	[TRANSPORT_VAR_REC_MEDIUM] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_PLAY_MEDIA] = "NETWORK,UNKNOWN",
	[TRANSPORT_VAR_REC_MEDIA] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_CUR_PLAY_MODE] = "NORMAL",
	[TRANSPORT_VAR_TRANSPORT_PLAY_SPEED] = "1",
	[TRANSPORT_VAR_REC_MEDIUM_WR_STATUS] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_CUR_REC_QUAL_MODE] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_POS_REC_QUAL_MODE] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_NR_TRACKS] = "1",
	[TRANSPORT_VAR_CUR_TRACK] = "1",
	[TRANSPORT_VAR_CUR_TRACK_DUR] = "00:00:00",
	[TRANSPORT_VAR_CUR_MEDIA_DUR] = "00:00:00",
	[TRANSPORT_VAR_CUR_TRACK_META] = "",
	[TRANSPORT_VAR_CUR_TRACK_URI] = "",
	[TRANSPORT_VAR_AV_URI] = "",
	[TRANSPORT_VAR_AV_URI_META] = "",
	[TRANSPORT_VAR_NEXT_AV_URI] = "",
	[TRANSPORT_VAR_NEXT_AV_URI_META] = "",
	[TRANSPORT_VAR_REL_TIME_POS] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_ABS_TIME_POS] = "NOT_IMPLEMENTED",
	[TRANSPORT_VAR_REL_CTR_POS] = "2147483647",
	[TRANSPORT_VAR_ABS_CTR_POS] = "2147483647",
	[TRANSPORT_VAR_LAST_CHANGE] = "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\"><InstanceID val=\"0\"></InstanceID></Event>",
	[TRANSPORT_VAR_AAT_SEEK_MODE] = "TRACK_NR",
	[TRANSPORT_VAR_AAT_SEEK_TARGET] = "",
	[TRANSPORT_VAR_AAT_INSTANCE_ID] = "0",
	[TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS] = "Play,Stop,Pause",
	[TRANSPORT_VAR_UNKNOWN] = NULL
};

static char *transport_values[sizeof(transport_defaults) / sizeof(char *)];

static const char *transport_states[] =
{
	"STOPPED",
	"PAUSED_PLAYBACK",
	"PAUSED_RECORDING",
	"PLAYING",
	"RECORDING",
	"TRANSITIONING",
	"NO_MEDIA_PRESENT",
	NULL
};
static const char *transport_stati[] =
{
	"OK",
	"ERROR_OCCURRED",
	" vendor-defined ",
	NULL
};
static const char *media[] =
{
	"UNKNOWN",
	"DV",
	"MINI-DV",
	"VHS",
	"W-VHS",
	"S-VHS",
	"D-VHS",
	"VHSC",
	"VIDEO8",
	"HI8",
	"CD-ROM",
	"CD-DA",
	"CD-R",
	"CD-RW",
	"VIDEO-CD",
	"SACD",
	"MD-AUDIO",
	"MD-PICTURE",
	"DVD-ROM",
	"DVD-VIDEO",
	"DVD-R",
	"DVD+RW",
	"DVD-RW",
	"DVD-RAM",
	"DVD-AUDIO",
	"DAT",
	"LD",
	"HDD",
	"MICRO-MV",
	"NETWORK",
	"NONE",
	"NOT_IMPLEMENTED",
	" vendor-defined ",
	NULL
};

static const char *playmodi[] =
{
	"NORMAL",
	//"SHUFFLE",
	"REPEAT_ONE",
	"REPEAT_ALL",
	"RANDOM",
	"DIRECT_1",
	//"INTRO",
	NULL
};

static const char *playspeeds[] =
{
	"1",
	" vendor-defined ",
	NULL
};

static const char *rec_write_stati[] =
{
	"WRITABLE",
	"PROTECTED",
	"NOT_WRITABLE",
	"UNKNOWN",
	"NOT_IMPLEMENTED",
	NULL
};

static const char *rec_quality_modi[] =
{
	"0:EP",
	"1:LP",
	"2:SP",
	"0:BASIC",
	"1:MEDIUM",
	"2:HIGH",
	"NOT_IMPLEMENTED",
	" vendor-defined ",
	NULL
};

static const char *aat_seekmodi[] =
{
	"ABS_TIME",
	"REL_TIME",
	"ABS_COUNT",
	"REL_COUNT",
	"TRACK_NR",
	"CHANNEL_FREQ",
	"TAPE-INDEX",
	"FRAME",
	NULL
};

static struct param_range track_range =
{
	0,
	4294967295LL,
	1
};

static struct param_range track_nr_range =
{
	0,
	4294967295LL,
	0
};

static struct var_meta transport_var_meta[] =
{
	[TRANSPORT_VAR_TRANSPORT_STATE] =		{ SENDEVENT_NO, DATATYPE_STRING, transport_states, NULL },
	[TRANSPORT_VAR_TRANSPORT_STATUS] =		{ SENDEVENT_NO, DATATYPE_STRING, transport_stati, NULL },
	[TRANSPORT_VAR_PLAY_MEDIUM] =			{ SENDEVENT_NO, DATATYPE_STRING, media, NULL },
	[TRANSPORT_VAR_REC_MEDIUM] =			{ SENDEVENT_NO, DATATYPE_STRING, media, NULL },
	[TRANSPORT_VAR_PLAY_MEDIA] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_REC_MEDIA] =			    { SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_PLAY_MODE] =			{ SENDEVENT_NO, DATATYPE_STRING, playmodi, NULL, "NORMAL" },
	[TRANSPORT_VAR_TRANSPORT_PLAY_SPEED] =	{ SENDEVENT_NO, DATATYPE_STRING, playspeeds, NULL },
	[TRANSPORT_VAR_REC_MEDIUM_WR_STATUS] =	{ SENDEVENT_NO, DATATYPE_STRING, rec_write_stati, NULL },
	[TRANSPORT_VAR_CUR_REC_QUAL_MODE] =		{ SENDEVENT_NO, DATATYPE_STRING, rec_quality_modi, NULL },
	[TRANSPORT_VAR_POS_REC_QUAL_MODE] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_NR_TRACKS] =			    { SENDEVENT_NO, DATATYPE_UI4, NULL, &track_nr_range }, /* no step */
	[TRANSPORT_VAR_CUR_TRACK] =			    { SENDEVENT_NO, DATATYPE_UI4, NULL, &track_range },
	[TRANSPORT_VAR_CUR_TRACK_DUR] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_MEDIA_DUR] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_TRACK_META] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_CUR_TRACK_URI] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AV_URI] =			    { SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AV_URI_META] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_NEXT_AV_URI] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_NEXT_AV_URI_META] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_REL_TIME_POS] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_ABS_TIME_POS] =			{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_REL_CTR_POS] =			{ SENDEVENT_NO, DATATYPE_I4, NULL, NULL },
	[TRANSPORT_VAR_ABS_CTR_POS] =			{ SENDEVENT_NO, DATATYPE_I4, NULL, NULL },
	[TRANSPORT_VAR_LAST_CHANGE] =			{ SENDEVENT_YES, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AAT_SEEK_MODE] =			{ SENDEVENT_NO, DATATYPE_STRING, aat_seekmodi, NULL },
	[TRANSPORT_VAR_AAT_SEEK_TARGET] =		{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_AAT_INSTANCE_ID] =		{ SENDEVENT_NO, DATATYPE_UI4, NULL, NULL },
	[TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS] =	{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[TRANSPORT_VAR_UNKNOWN] =			    { SENDEVENT_NO, DATATYPE_UNKNOWN, NULL, NULL }
};

static struct argument *arguments_setavtransporturi[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "CurrentURI", PARAM_DIR_IN, TRANSPORT_VAR_AV_URI },
	& (struct argument) { "CurrentURIMetaData", PARAM_DIR_IN, TRANSPORT_VAR_AV_URI_META },
	NULL
};

//static struct argument *arguments_setnextavtransporturi[] = {
//        & (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
//        & (struct argument) { "NextURI", PARAM_DIR_IN, TRANSPORT_VAR_NEXT_AV_URI },
//        & (struct argument) { "NextURIMetaData", PARAM_DIR_IN, TRANSPORT_VAR_NEXT_AV_URI_META },
//        NULL
//};

static struct argument *arguments_getmediainfo[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "NrTracks", PARAM_DIR_OUT, TRANSPORT_VAR_NR_TRACKS },
	& (struct argument) { "MediaDuration", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_MEDIA_DUR },
	& (struct argument) { "CurrentURI", PARAM_DIR_OUT, TRANSPORT_VAR_AV_URI },
	& (struct argument) { "CurrentURIMetaData", PARAM_DIR_OUT, TRANSPORT_VAR_AV_URI_META },
	& (struct argument) { "NextURI", PARAM_DIR_OUT, TRANSPORT_VAR_NEXT_AV_URI },
	& (struct argument) { "NextURIMetaData", PARAM_DIR_OUT, TRANSPORT_VAR_NEXT_AV_URI_META },
	& (struct argument) { "PlayMedium", PARAM_DIR_OUT, TRANSPORT_VAR_PLAY_MEDIUM },
	& (struct argument) { "RecordMedium", PARAM_DIR_OUT, TRANSPORT_VAR_REC_MEDIUM },
	& (struct argument) { "WriteStatus", PARAM_DIR_OUT, TRANSPORT_VAR_REC_MEDIUM_WR_STATUS },
	NULL
};

static struct argument *arguments_gettransportinfo[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "CurrentTransportState", PARAM_DIR_OUT, TRANSPORT_VAR_TRANSPORT_STATE },
	& (struct argument) { "CurrentTransportStatus", PARAM_DIR_OUT, TRANSPORT_VAR_TRANSPORT_STATUS },
	& (struct argument) { "CurrentSpeed", PARAM_DIR_OUT, TRANSPORT_VAR_TRANSPORT_PLAY_SPEED },
	NULL
};

static struct argument *arguments_getpositioninfo[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Track", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK },
	& (struct argument) { "TrackDuration", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK_DUR },
	& (struct argument) { "TrackMetaData", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK_META },
	& (struct argument) { "TrackURI", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRACK_URI },
	& (struct argument) { "RelTime", PARAM_DIR_OUT, TRANSPORT_VAR_REL_TIME_POS },
	& (struct argument) { "AbsTime", PARAM_DIR_OUT, TRANSPORT_VAR_ABS_TIME_POS },
	& (struct argument) { "RelCount", PARAM_DIR_OUT, TRANSPORT_VAR_REL_CTR_POS },
	& (struct argument) { "AbsCount", PARAM_DIR_OUT, TRANSPORT_VAR_ABS_CTR_POS },
	NULL
};

static struct argument *arguments_getdevicecapabilities[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "PlayMedia", PARAM_DIR_OUT, TRANSPORT_VAR_PLAY_MEDIA },
	& (struct argument) { "RecMedia", PARAM_DIR_OUT, TRANSPORT_VAR_REC_MEDIA },
	& (struct argument) { "RecQualityModes", PARAM_DIR_OUT, TRANSPORT_VAR_POS_REC_QUAL_MODE },
	NULL
};

static struct argument *arguments_gettransportsettings[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "PlayMode", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_PLAY_MODE },
	& (struct argument) { "RecQualityMode", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_REC_QUAL_MODE },
	NULL
};

static struct argument *arguments_stop[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
static struct argument *arguments_play[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Speed", PARAM_DIR_IN, TRANSPORT_VAR_TRANSPORT_PLAY_SPEED },
	NULL
};
static struct argument *arguments_pause[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
//static struct argument *arguments_record[] = {
//        & (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
//	NULL
//};

static struct argument *arguments_seek[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Unit", PARAM_DIR_IN, TRANSPORT_VAR_AAT_SEEK_MODE },
	& (struct argument) { "Target", PARAM_DIR_IN, TRANSPORT_VAR_AAT_SEEK_TARGET },
	NULL
};
static struct argument *arguments_next[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
static struct argument *arguments_previous[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	NULL
};
static struct argument *arguments_setplaymode[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "NewPlayMode", PARAM_DIR_IN, TRANSPORT_VAR_CUR_PLAY_MODE },
	NULL
};
//static struct argument *arguments_setrecordqualitymode[] = {
//        & (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
//        & (struct argument) { "NewRecordQualityMode", PARAM_DIR_IN, TRANSPORT_VAR_CUR_REC_QUAL_MODE },
//	NULL
//};
static struct argument *arguments_getcurrenttransportactions[] =
{
	& (struct argument) { "InstanceID", PARAM_DIR_IN, TRANSPORT_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Actions", PARAM_DIR_OUT, TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS },
	NULL
};


static struct argument **argument_list[] =
{
	[TRANSPORT_CMD_SETAVTRANSPORTURI] =         arguments_setavtransporturi,
	[TRANSPORT_CMD_GETDEVICECAPABILITIES] =     arguments_getdevicecapabilities,
	[TRANSPORT_CMD_GETMEDIAINFO] =              arguments_getmediainfo,
	//[TRANSPORT_CMD_SETNEXTAVTRANSPORTURI] =     arguments_setnextavtransporturi,
	[TRANSPORT_CMD_GETTRANSPORTINFO] =          arguments_gettransportinfo,
	[TRANSPORT_CMD_GETPOSITIONINFO] =           arguments_getpositioninfo,
	[TRANSPORT_CMD_GETTRANSPORTSETTINGS] =      arguments_gettransportsettings,
	[TRANSPORT_CMD_STOP] =                      arguments_stop,
	[TRANSPORT_CMD_PLAY] =                      arguments_play,
	[TRANSPORT_CMD_PAUSE] =                     arguments_pause,
	//[TRANSPORT_CMD_RECORD] =                    arguments_record,
	[TRANSPORT_CMD_SEEK] =                      arguments_seek,
	[TRANSPORT_CMD_NEXT] =                      arguments_next,
	[TRANSPORT_CMD_PREVIOUS] =                  arguments_previous,
	[TRANSPORT_CMD_SETPLAYMODE] =               arguments_setplaymode,
	//[TRANSPORT_CMD_SETRECORDQUALITYMODE] =      arguments_setrecordqualitymode,
	[TRANSPORT_CMD_GETCURRENTTRANSPORTACTIONS] = arguments_getcurrenttransportactions,
	[TRANSPORT_CMD_UNKNOWN] =	NULL
};

/* protects transport_values, and service-specific state */

static ithread_mutex_t transport_mutex = PTHREAD_MUTEX_INITIALIZER;

static enum _transport_state transport_state = -1;

static int get_media_info(struct action_event *event)
{
	int rc = -1;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	rc = upnp_append_variable(event, TRANSPORT_VAR_NR_TRACKS, "NrTracks");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_MEDIA_DUR, "MediaDuration");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_AV_URI, "CurrentURI");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_AV_URI_META, "CurrentURIMetaData");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_NEXT_AV_URI, "NextURI");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_NEXT_AV_URI_META, "NextURIMetaData");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REC_MEDIA, "PlayMedium");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REC_MEDIUM, "RecordMedium");
	if (rc)
		goto out;

	rc = upnp_append_variable(event,
				  TRANSPORT_VAR_REC_MEDIUM_WR_STATUS,
				  "WriteStatus");
	if (rc)
		goto out;

out:
	return rc;
}

DBG_STATIC void transport_notify_lastchange(struct action_event *event, char *value)
{
	const char *varnames[] =
	{
		"LastChange",
		NULL
	};
	char *varvalues[] =
	{
		NULL, NULL
	};

	DBG_PRINT(DBG_LVL4, "AVT Event: '%s'\n", value);
	varvalues[0] = xmlescape(value, 0);

	if (transport_values[TRANSPORT_VAR_LAST_CHANGE])
		free(transport_values[TRANSPORT_VAR_LAST_CHANGE]);

	// Save arg
	transport_values[TRANSPORT_VAR_LAST_CHANGE] = value;
	UpnpNotify(device_handle, event->request->DevUDN,
		   event->request->ServiceID,
		   varnames, (const char **)varvalues, 1);

	free(varvalues[0]);
}

void transport_set_var(int varnum, char *value)
{
	assert((varnum >= 0) && (varnum < TRANSPORT_VAR_UNKNOWN));

	if (value == NULL)
		return;

	if (transport_values[varnum])
	{
		// Any change - return if identical
		if (strcmp(transport_values[varnum], value) == 0)
			return;
		// Free old value before saving new one
		free(transport_values[varnum]);
	}

	transport_values[varnum] = strdup(value);

	return;
}

char *transport_get_var(int varnum)
{
	assert((varnum >= 0) && (varnum < TRANSPORT_VAR_UNKNOWN));

	return transport_values[varnum];
}

void transport_set_state(enum _transport_state state, char *value)
{
	transport_state = state;
	transport_set_var(TRANSPORT_VAR_TRANSPORT_STATE, value);

	return;
}

/* warning - does not lock service mutex */
DBG_STATIC void transport_change_var(struct action_event *event, int varnum, char *new_value)
{
	char *buf;

	transport_set_var(varnum, new_value);

	asprintf(&buf,
		 "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\">"
		 "<InstanceID val=\"0\"><%s val=\"%s\"/></InstanceID></Event>",
		 transport_variables[varnum], transport_values[varnum]);

	transport_notify_lastchange(event, buf);

	return;
}

DBG_STATIC char *transport_get_state_lastchange(void)
{
	char *buf;

	// Construct LastChange event accordingly
	if (strcmp(transport_values[TRANSPORT_VAR_CUR_TRACK_URI], "") == 0)
	{
		asprintf(&buf,
			 "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\">"
			 "<InstanceID val=\"0\">"
			 "<%s val=\"%s\"/><%s val=\"%s\"/>"
			 "</InstanceID></Event>",
			 transport_variables[TRANSPORT_VAR_TRANSPORT_STATE], transport_values[TRANSPORT_VAR_TRANSPORT_STATE],
			 transport_variables[TRANSPORT_VAR_TRANSPORT_STATUS], transport_values[TRANSPORT_VAR_TRANSPORT_STATUS]);
	}
	else
	{
		asprintf(&buf,
			 "<Event xmlns=\"urn:schemas-upnp-org:metadata-1-0/AVT/\">"
			 "<InstanceID val=\"0\">"
			 "<%s val=\"%s\"/><%s val=\"%s\"/><%s val=\"%s\"/><%s val=\"%s\"/><%s val=\"%s\"/>"
			 "</InstanceID></Event>",
			 transport_variables[TRANSPORT_VAR_TRANSPORT_STATE], transport_values[TRANSPORT_VAR_TRANSPORT_STATE],
			 transport_variables[TRANSPORT_VAR_TRANSPORT_STATUS], transport_values[TRANSPORT_VAR_TRANSPORT_STATUS],
			 transport_variables[TRANSPORT_VAR_CUR_TRACK_URI], transport_values[TRANSPORT_VAR_CUR_TRACK_URI],
			 transport_variables[TRANSPORT_VAR_CUR_TRACK_META], transport_values[TRANSPORT_VAR_CUR_TRACK_META],
			 transport_variables[TRANSPORT_VAR_CUR_TRACK_DUR], transport_values[TRANSPORT_VAR_CUR_TRACK_DUR]);
	}


	return buf;
}

// Extract attribute from URI_METADATA
const char *transport_get_attr_metadata(const char *key)
{
	IXML_Document *doc;
	IXML_Element *elm;
	const char *attr = NULL;

	const char *metadata = transport_get_var(TRANSPORT_VAR_AV_URI_META);

	if (!metadata || (strcmp(metadata, "") == 0))
		return NULL;

	doc = ixmlParseBuffer(metadata);
	if (doc)
	{
		// Locate 'res' element
		elm = ixmlDocument_getElementById(doc, "res");
		if (elm != NULL)
		{
			// Look for 'key' attribute in 'res' element
			attr = ixmlElement_getAttribute(elm, key);
			// Copy of result (caller must free)
			if (attr)
				attr = strdup(attr);
		}

		// Free the whole DOM
		ixmlDocument_free(doc);
	}

	DBG_PRINT(DBG_LVL1, "Track metadata: %s = %s\n", key, (attr) ? attr : "<not found>");

	// NULL if not found
	return attr;
}

/* UPnP action handlers */

DBG_STATIC int set_avtransport_uri(struct action_event *event)
{
	char *value;
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	value = upnp_get_string(event, "CurrentURI");
	if (value == NULL)
		return -1;

	ithread_mutex_lock(&transport_mutex);

	DBG_PRINT(DBG_LVL4, "%s: Set URI to '%s'\n", __FUNCTION__, value);

	// do the work
	output_set_uri(value);

	transport_set_var(TRANSPORT_VAR_AV_URI, value);

	free(value);

	value = upnp_get_string(event, "CurrentURIMetaData");
	if (value != NULL)
	{
		DBG_PRINT(DBG_LVL4, "%s: Set URI MetaData to '%s'\n", __FUNCTION__, value);

		// First set new value so we may extract some information about
		// the stream (Ex: track_duration)
		transport_set_var(TRANSPORT_VAR_AV_URI_META, value);
		free(value);

		// Locate duration attribute in 'res' element under 'item'
		value = (char *)transport_get_attr_metadata("duration");
		if (value)
		{
			transport_set_var(TRANSPORT_VAR_CUR_TRACK_DUR, value);
			free(value);
		}
	}
	else
	{
		rc = -1;
	}

	transport_state = TRANSPORT_STOPPED;
	transport_set_var(TRANSPORT_VAR_TRANSPORT_STATE, "STOPPED");

	transport_notify_lastchange(event, transport_get_state_lastchange());

	ithread_mutex_unlock(&transport_mutex);

	return rc;
}

//DBG_STATIC int set_next_avtransport_uri(struct action_event *event)
//{
//	char *value;
//
//	if (upnp_obtain_instanceid(event, NULL))
//	{
//	    upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
//		return -1;
//	}
//
//	value = upnp_get_string(event, "NextURI");
//	if (value == NULL) {
//		return -1;
//	}
//	DBG_PRINT(DBG_LVL4, "%s: NextURI='%s'\n", __FUNCTION__, value);
//	free(value);
//	value = upnp_get_string(event, "NextURIMetaData");
//	if (value == NULL) {
//		return -1;
//	}
//	DBG_PRINT(DBG_LVL4, "%s: NextURIMetaData='%s'\n", __FUNCTION__, value);
//	free(value);
//
//	return 0;
//}

DBG_STATIC int get_transport_info(struct action_event *event)
{
	int rc = -1;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		//return -1;
	}

	rc = upnp_append_variable(event, TRANSPORT_VAR_TRANSPORT_STATE, "CurrentTransportState");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_TRANSPORT_STATUS, "CurrentTransportStatus");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_TRANSPORT_PLAY_SPEED, "CurrentSpeed");
	if (rc)
		goto out;

out:
	return rc;
}

DBG_STATIC int get_transport_settings(struct action_event *event)
{
	int rc = -1;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_PLAY_MODE, "CurrentPlayMode");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_REC_QUAL_MODE, "CurrentRecordQualityMode");
	if (rc)
		goto out;

out:
	return rc;
}

DBG_STATIC int get_position_info(struct action_event *event)
{
	int rc = -1;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	ithread_mutex_lock(&transport_mutex);

	// Calls back into transport to set vars
	output_update_position();

	ithread_mutex_unlock(&transport_mutex);

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK, "Track");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK_DUR, "TrackDuration");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK_META, "TrackMetaData");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_CUR_TRACK_URI, "TrackURI");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REL_TIME_POS, "RelTime");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_ABS_TIME_POS, "AbsTime");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_REL_CTR_POS, "RelCount");
	if (rc)
		goto out;

	rc = upnp_append_variable(event, TRANSPORT_VAR_ABS_CTR_POS, "AbsCount");
	if (rc)
		goto out;

out:
	return rc;
}

DBG_STATIC int get_device_caps(struct action_event *event)
{
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	// *** TODO: Add some information

	return rc;
}

DBG_STATIC int xplaymode(struct action_event *event)
{
	char *newmode;
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	newmode = upnp_get_string(event, "NewPlayMode");
	DBG_PRINT(DBG_LVL4, "Set NewPlayMode: %s\n", newmode);

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	ithread_mutex_lock(&transport_mutex);

	rc = output_playmode(newmode);
	if (rc != 0)
	{
		free(newmode);
		upnp_set_error(event, UPNP_TRANSPORT_E_PLAYMODE_NS, "Set playmode failed");
		goto out;
	}

	transport_change_var(event, TRANSPORT_VAR_CUR_PLAY_MODE, newmode);
	free(newmode);

out:
	ithread_mutex_unlock(&transport_mutex);

	return rc;
}

DBG_STATIC int xstop(struct action_event *event)
{
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	ithread_mutex_lock(&transport_mutex);

	switch (transport_state)
	{
	case TRANSPORT_STOPPED:
		break;
	case TRANSPORT_PLAYING:
	case TRANSPORT_TRANSITIONING:
	case TRANSPORT_PAUSED_PLAYBACK:
		if (output_stop())
		{
			upnp_set_error(event, UPNP_TRANSPORT_E_NO_CONTENTS, "Player Stop failed");
			rc = -1;
		}
		else
		{
			transport_state = TRANSPORT_STOPPED;
			transport_change_var(event, TRANSPORT_VAR_TRANSPORT_STATE, "STOPPED");
		}
		// Set TransportPlaySpeed to '1'
		break;

	case TRANSPORT_NO_MEDIA_PRESENT:
		/* action not allowed in these states - error 701 */
		upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA, "Transition not allowed");

		break;
	}

	ithread_mutex_unlock(&transport_mutex);

	return rc;
}

DBG_STATIC int xpause(struct action_event *event)
{
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	ithread_mutex_lock(&transport_mutex);

	switch (transport_state)
	{
	case TRANSPORT_STOPPED:
		break;
	case TRANSPORT_PLAYING:
		if (output_pause())
		{
			upnp_set_error(event, UPNP_TRANSPORT_E_NO_CONTENTS, "Player Pause failed");
			rc = -1;
		}
		else
		{
			transport_state = TRANSPORT_PAUSED_PLAYBACK;
			transport_change_var(event, TRANSPORT_VAR_TRANSPORT_STATE, "PAUSED_PLAYBACK");
		}
		break;

	case TRANSPORT_PAUSED_PLAYBACK:
		if (output_play())
		{
			upnp_set_error(event, UPNP_TRANSPORT_E_NO_CONTENTS, "Player Start failed");
			rc = -1;
		}
		else
		{
			transport_state = TRANSPORT_PLAYING;
			transport_change_var(event, TRANSPORT_VAR_TRANSPORT_STATE, "PLAYING");
		}
		break;

	case TRANSPORT_TRANSITIONING:
	case TRANSPORT_NO_MEDIA_PRESENT:
		/* action not allowed in these states - error 701 */
		upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA, "Transition not allowed");
		break;
	}

	ithread_mutex_unlock(&transport_mutex);

	return rc;
}

DBG_STATIC int xplay(struct action_event *event)
{
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	ithread_mutex_lock(&transport_mutex);

	switch (transport_state)
	{
	case TRANSPORT_PLAYING:
		// Set TransportPlaySpeed to '1'
		break;

	case TRANSPORT_STOPPED:
	case TRANSPORT_PAUSED_PLAYBACK:
		if (output_play())
		{
			upnp_set_error(event, UPNP_TRANSPORT_E_NO_CONTENTS, "Player Start failed");
			rc = -1;
		}
		else
		{
			transport_state = TRANSPORT_PLAYING;
			transport_change_var(event, TRANSPORT_VAR_TRANSPORT_STATE, "PLAYING");

		}
		// Set TransportPlaySpeed to '1'
		break;

	case TRANSPORT_NO_MEDIA_PRESENT:
	case TRANSPORT_TRANSITIONING:
		/* action not allowed in these states - error 701 */
		upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA,
			       "Transition not allowed");
		rc = -1;

		break;
	}

	ithread_mutex_unlock(&transport_mutex);

	return rc;
}

DBG_STATIC int xseek(struct action_event *event)
{
	char *value, *mode;
	int rc = 0;

	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}


	mode = upnp_get_string(event, "Unit");
	if (mode == NULL)
		return -1;

	value = upnp_get_string(event, "Target");
	if (value == NULL)
	{
		free(mode);
		return -1;
	}

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	// Attempt to seek player (doesn't work for streams)
	rc = output_seekto(mode, value);
	free(mode);
	free(value);
	if (rc != 0)
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_ILL_SEEKTARGET, "Player Seek failed");
		return -1;
	}

	return rc;
}

DBG_STATIC int xnext(struct action_event *event)
{
	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	if (output_next())
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA, "Player Next failed");
		return -1;
	}

	return 0;
}

DBG_STATIC int xprevious(struct action_event *event)
{
	if (upnp_obtain_instanceid(event, NULL))
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_INVALID_IID, "ID non-zero invalid");
		return -1;
	}

	// Check MPD connection
	if (check_mpd_connection(TRUE) == STATUS_FAIL)
		return -1;

	if (output_prev())
	{
		upnp_set_error(event, UPNP_TRANSPORT_E_TRANSITION_NA, "Player Previous failed");
		return -1;
	}

	return 0;
}

DBG_STATIC int transport_notify_subscription(void)
{
	ithread_mutex_lock(&transport_mutex);

	output_update_status();

	transport_set_var(TRANSPORT_VAR_LAST_CHANGE, transport_get_state_lastchange());

	ithread_mutex_unlock(&transport_mutex);

	return 0;
}

static struct action transport_actions[] =
{
	[TRANSPORT_CMD_GETCURRENTTRANSPORTACTIONS] = {"GetCurrentTransportActions", NULL},	/* optional */
	[TRANSPORT_CMD_GETDEVICECAPABILITIES] =     {"GetDeviceCapabilities", get_device_caps},
	[TRANSPORT_CMD_GETMEDIAINFO] =              {"GetMediaInfo", get_media_info},
	[TRANSPORT_CMD_SETAVTRANSPORTURI] =         {"SetAVTransportURI", set_avtransport_uri},	/* RC9800i */
	//[TRANSPORT_CMD_SETNEXTAVTRANSPORTURI] =   {"SetNextAVTransportURI", set_next_avtransport_uri},
	[TRANSPORT_CMD_GETTRANSPORTINFO] =          {"GetTransportInfo", get_transport_info},
	[TRANSPORT_CMD_GETPOSITIONINFO] =           {"GetPositionInfo", get_position_info},
	[TRANSPORT_CMD_GETTRANSPORTSETTINGS] =      {"GetTransportSettings", get_transport_settings},
	[TRANSPORT_CMD_STOP] =                      {"Stop", xstop},
	[TRANSPORT_CMD_PLAY] =                      {"Play", xplay},
	[TRANSPORT_CMD_PAUSE] =                     {"Pause", xpause},
	//[TRANSPORT_CMD_RECORD] =                  {"Record", NULL},	/* optional */
	[TRANSPORT_CMD_SEEK] =                      {"Seek", xseek},
	[TRANSPORT_CMD_NEXT] =                      {"Next", xnext},
	[TRANSPORT_CMD_PREVIOUS] =                  {"Previous", xprevious},
	[TRANSPORT_CMD_SETPLAYMODE] =               {"SetPlayMode", xplaymode},
	//[TRANSPORT_CMD_SETRECORDQUALITYMODE] =    {"SetRecordQualityMode", NULL},	/* optional */
	[TRANSPORT_CMD_UNKNOWN] =                   {NULL, NULL}
};


struct service transport_service =
{
	.service_name =         TRANSPORT_SERVICE,
	.type =                 TRANSPORT_TYPE,
	.scpd_url =		        TRANSPORT_SCPD_URL,
	.control_url =		    TRANSPORT_CONTROL_URL,
	.event_url =		    TRANSPORT_EVENT_URL,
	.actions =              transport_actions,
	.action_arguments =     argument_list,
	.variable_names =       transport_variables,
	.variable_values =      transport_values,
	.variable_defaults =    transport_defaults,
	.variable_meta =        transport_var_meta,
	.variable_count =       TRANSPORT_VAR_UNKNOWN,
	.command_count =        TRANSPORT_CMD_UNKNOWN,
	.service_mutex =        &transport_mutex,
	.subscription_notify =  &transport_notify_subscription
};

void transport_init(void)
{
	// init values array
	memset(transport_values, 0, sizeof(transport_values));

	// Some things that we will need early
	transport_values[TRANSPORT_VAR_TRANSPORT_STATE] = strdup(transport_defaults[TRANSPORT_VAR_TRANSPORT_STATE]);
	transport_values[TRANSPORT_VAR_TRANSPORT_STATUS] = strdup(transport_defaults[TRANSPORT_VAR_TRANSPORT_STATUS]);
	transport_values[TRANSPORT_VAR_CUR_TRACK_URI] = strdup(transport_defaults[TRANSPORT_VAR_CUR_TRACK_URI]);
	transport_values[TRANSPORT_VAR_CUR_TRACK_META] = strdup(transport_defaults[TRANSPORT_VAR_CUR_TRACK_META]);
	transport_values[TRANSPORT_VAR_CUR_TRACK_DUR] = strdup(transport_defaults[TRANSPORT_VAR_CUR_TRACK_DUR]);

	return;
}
