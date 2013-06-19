/* upnp_control.h - UPnP RenderingControl definitions
 *
 * Copyright (C) 2005   Ivo Clarysse
 * Copyright (C) 2012	Ted Hess (Kitschensync)
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

#ifndef _UPNP_CONTROL_H
#define _UPNP_CONTROL_H

#define UPNP_CONTROL_E_INVALID_IID	702

#if defined(UPNP_VIDEO)
typedef enum {
	CONTROL_VAR_G_GAIN,
	CONTROL_VAR_B_BLACK,
	CONTROL_VAR_VER_KEYSTONE,
	CONTROL_VAR_G_BLACK,
	CONTROL_VAR_VOLUME,
	CONTROL_VAR_LOUDNESS,
	CONTROL_VAR_AAT_INSTANCE_ID,
	CONTROL_VAR_R_GAIN,
	CONTROL_VAR_COLOR_TEMP,
	CONTROL_VAR_SHARPNESS,
	CONTROL_VAR_AAT_PRESET_NAME,
	CONTROL_VAR_R_BLACK,
	CONTROL_VAR_B_GAIN,
	CONTROL_VAR_MUTE,
	CONTROL_VAR_LAST_CHANGE,
	CONTROL_VAR_AAT_CHANNEL,
	CONTROL_VAR_HOR_KEYSTONE,
	CONTROL_VAR_VOLUME_DB,
	CONTROL_VAR_PRESET_NAME_LIST,
	CONTROL_VAR_CONTRAST,
	CONTROL_VAR_BRIGHTNESS,
	CONTROL_VAR_UNKNOWN,
	CONTROL_VAR_COUNT
} control_variable;

typedef enum {
	CONTROL_CMD_GET_BLUE_BLACK,
	CONTROL_CMD_GET_BLUE_GAIN,
	CONTROL_CMD_GET_BRIGHTNESS,
	CONTROL_CMD_GET_COLOR_TEMP,
	CONTROL_CMD_GET_CONTRAST,
	CONTROL_CMD_GET_GREEN_BLACK,
	CONTROL_CMD_GET_GREEN_GAIN,
	CONTROL_CMD_GET_HOR_KEYSTONE,
	CONTROL_CMD_GET_LOUDNESS,
	CONTROL_CMD_GET_MUTE,
	CONTROL_CMD_GET_RED_BLACK,
	CONTROL_CMD_GET_RED_GAIN,
	CONTROL_CMD_GET_SHARPNESS,
	CONTROL_CMD_GET_VERT_KEYSTONE,
	CONTROL_CMD_GET_VOL,
	CONTROL_CMD_GET_VOL_DB,
	CONTROL_CMD_GET_VOL_DBRANGE,
	CONTROL_CMD_LIST_PRESETS,
	CONTROL_CMD_SELECT_PRESET,
	CONTROL_CMD_SET_BLUE_BLACK,
	CONTROL_CMD_SET_BLUE_GAIN,
	CONTROL_CMD_SET_BRIGHTNESS,
	CONTROL_CMD_SET_COLOR_TEMP,
	CONTROL_CMD_SET_CONTRAST,
	CONTROL_CMD_SET_GREEN_BLACK,
	CONTROL_CMD_SET_GREEN_GAIN,
	CONTROL_CMD_SET_HOR_KEYSTONE,
	CONTROL_CMD_SET_LOUDNESS,
	CONTROL_CMD_SET_MUTE,
	CONTROL_CMD_SET_RED_BLACK,
	CONTROL_CMD_SET_RED_GAIN,
	CONTROL_CMD_SET_SHARPNESS,
	CONTROL_CMD_SET_VERT_KEYSTONE,
	CONTROL_CMD_SET_VOL,
	CONTROL_CMD_SET_VOL_DB,
	CONTROL_CMD_UNKNOWN,
	CONTROL_CMD_COUNT
} control_cmd;

#else
// Audio device only
typedef enum {
	CONTROL_VAR_VOLUME,
	CONTROL_VAR_LOUDNESS,
	CONTROL_VAR_AAT_INSTANCE_ID,
	CONTROL_VAR_AAT_PRESET_NAME,
	CONTROL_VAR_MUTE,
	CONTROL_VAR_LAST_CHANGE,
	CONTROL_VAR_AAT_CHANNEL,
	CONTROL_VAR_VOLUME_DB,
	CONTROL_VAR_PRESET_NAME_LIST,
	CONTROL_VAR_UNKNOWN,
	CONTROL_VAR_COUNT
} control_variable;

typedef enum {
	CONTROL_CMD_GET_LOUDNESS,
	CONTROL_CMD_GET_MUTE,
	CONTROL_CMD_GET_VOL,
	CONTROL_CMD_GET_VOL_DB,
	CONTROL_CMD_GET_VOL_DBRANGE,
	CONTROL_CMD_LIST_PRESETS,
	CONTROL_CMD_SELECT_PRESET,
	CONTROL_CMD_SET_LOUDNESS,
	CONTROL_CMD_SET_MUTE,
	CONTROL_CMD_SET_VOL,
	CONTROL_CMD_SET_VOL_DB,
	CONTROL_CMD_UNKNOWN,
	CONTROL_CMD_COUNT
} control_cmd;

#endif

struct action_event;

extern struct service control_service;

extern void control_init(void);
extern void control_set_var(int varnum, char *value);
extern char *control_get_var(int varnum);

#endif /* _UPNP_CONTROL_H */
