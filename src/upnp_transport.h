/* upnp_transport.h - UPnP AVTransport definitions
 *
 * Copyright (C) 2005   Ivo Clarysse
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

#ifndef _UPNP_TRANSPORT_H
#define _UPNP_TRANSPORT_H

#define UPNP_TRANSPORT_E_TRANSITION_NA	701
#define UPNP_TRANSPORT_E_NO_CONTENTS	702
#define UPNP_TRANSPORT_E_READ_ERROR 	703
#define UPNP_TRANSPORT_E_PLAY_FORMAT_NS	704
#define UPNP_TRANSPORT_E_TRANSPORT_LOCKED	705
#define UPNP_TRANSPORT_E_WRITE_ERROR	706
#define UPNP_TRANSPORT_E_REC_MEDIA_WP	707
#define UPNP_TRANSPORT_E_REC_FORMAT_NS	708
#define UPNP_TRANSPORT_E_REC_MEDIA_FULL	709
#define UPNP_TRANSPORT_E_SEEKMODE_NS	710
#define UPNP_TRANSPORT_E_ILL_SEEKTARGET	711
#define UPNP_TRANSPORT_E_PLAYMODE_NS	712
#define UPNP_TRANSPORT_E_RECQUAL_NS	    713
#define UPNP_TRANSPORT_E_ILLEGAL_MIME	714
#define UPNP_TRANSPORT_E_CONTENT_BUSY	715
#define UPNP_TRANSPORT_E_RES_NOT_FOUND	716
#define UPNP_TRANSPORT_E_PLAYSPEED_NS	717
#define UPNP_TRANSPORT_E_INVALID_IID	718

typedef enum
{
	TRANSPORT_VAR_TRANSPORT_STATUS,
	TRANSPORT_VAR_NEXT_AV_URI,
	TRANSPORT_VAR_NEXT_AV_URI_META,
	TRANSPORT_VAR_CUR_TRACK_META,
	TRANSPORT_VAR_REL_CTR_POS,
	TRANSPORT_VAR_AAT_INSTANCE_ID,
	TRANSPORT_VAR_AAT_SEEK_TARGET,
	TRANSPORT_VAR_PLAY_MEDIUM,
	TRANSPORT_VAR_REL_TIME_POS,
	TRANSPORT_VAR_REC_MEDIA,
	TRANSPORT_VAR_CUR_PLAY_MODE,
	TRANSPORT_VAR_TRANSPORT_PLAY_SPEED,
	TRANSPORT_VAR_PLAY_MEDIA,
	TRANSPORT_VAR_ABS_TIME_POS,
	TRANSPORT_VAR_CUR_TRACK,
	TRANSPORT_VAR_CUR_TRACK_URI,
	TRANSPORT_VAR_CUR_TRANSPORT_ACTIONS,
	TRANSPORT_VAR_NR_TRACKS,
	TRANSPORT_VAR_AV_URI,
	TRANSPORT_VAR_ABS_CTR_POS,
	TRANSPORT_VAR_CUR_REC_QUAL_MODE,
	TRANSPORT_VAR_CUR_MEDIA_DUR,
	TRANSPORT_VAR_AAT_SEEK_MODE,
	TRANSPORT_VAR_AV_URI_META,
	TRANSPORT_VAR_REC_MEDIUM,

	TRANSPORT_VAR_REC_MEDIUM_WR_STATUS,
	TRANSPORT_VAR_LAST_CHANGE,
	TRANSPORT_VAR_CUR_TRACK_DUR,
	TRANSPORT_VAR_TRANSPORT_STATE,
	TRANSPORT_VAR_POS_REC_QUAL_MODE,
	TRANSPORT_VAR_UNKNOWN,
	TRANSPORT_VAR_COUNT
} transport_variable;

typedef enum
{
	TRANSPORT_CMD_GETCURRENTTRANSPORTACTIONS,
	TRANSPORT_CMD_GETDEVICECAPABILITIES,
	TRANSPORT_CMD_GETMEDIAINFO,
	TRANSPORT_CMD_GETPOSITIONINFO,
	TRANSPORT_CMD_GETTRANSPORTINFO,
	TRANSPORT_CMD_GETTRANSPORTSETTINGS,
	TRANSPORT_CMD_NEXT,
	TRANSPORT_CMD_PAUSE,
	TRANSPORT_CMD_PLAY,
	TRANSPORT_CMD_PREVIOUS,
	TRANSPORT_CMD_SEEK,
	TRANSPORT_CMD_SETAVTRANSPORTURI,
	TRANSPORT_CMD_SETPLAYMODE,
	TRANSPORT_CMD_STOP,
	//TRANSPORT_CMD_SETNEXTAVTRANSPORTURI,
	//TRANSPORT_CMD_RECORD,
	//TRANSPORT_CMD_SETRECORDQUALITYMODE,
	TRANSPORT_CMD_UNKNOWN,
	TRANSPORT_CMD_COUNT
} transport_cmd ;

enum _transport_state
{
	TRANSPORT_STOPPED,
	TRANSPORT_PLAYING,
	TRANSPORT_TRANSITIONING,	/* optional */
	TRANSPORT_PAUSED_PLAYBACK,	/* optional */
//	TRANSPORT_PAUSED_RECORDING,	/* optional */
//	TRANSPORT_RECORDING,	/* optional */
	TRANSPORT_NO_MEDIA_PRESENT	/* optional */
};

extern struct service transport_service;

extern void transport_init(void);
extern void transport_set_var(int varnum, char *value);
extern void transport_set_state(enum _transport_state state, char *value);
extern char *transport_get_var(int varnum);
extern const char *transport_get_attr_metadata(const char *key);

#endif /* _UPNP_TRANSPORT_H */
