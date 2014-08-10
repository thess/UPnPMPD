/* output_mpd.h - Output module for MPD client definitions
 *
 * Copyright (C) 2012        Ted Hess (Kitschensync)
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

#ifndef _OUTPUT_MPD_H
#define _OUTPUT_MPD_H

#include <stdbool.h>
#include <libconfig.h>

int output_mpd_add_options(GOptionContext *ctx);
int output_mpd_init(config_t *cfg);
int output_play(void);
int output_stop(void);
int output_pause(void);
int output_loop(void);
int output_next(void);
int output_prev(void);
int output_playmode(const char *newmode);
int output_seekto(const char *seekmode, const char *seekpos);

void output_set_uri(const char *uri);
void output_set_mute(bool bmute);
void output_set_volume(const char *newvol);
void output_update_status(void);
void output_update_position(void);
extern const char *output_get_volume(void);

typedef enum
{
	STATUS_TEST = -1,
	STATUS_OK = 0,
	STATUS_FAIL = 1
} CNX_STATUS;

CNX_STATUS check_mpd_connection(bool update_status);

#endif /*  _OUTPUT_MPD_H */
