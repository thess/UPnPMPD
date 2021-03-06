/* upnp_renderer.h - UPnP renderer definitions
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

#ifndef _UPNP_RENDERER_H
#define _UPNP_RENDERER_H

#include <libconfig.h>

void upnp_renderer_dump_connmgr_scpd(void);
void upnp_renderer_dump_control_scpd(void);
void upnp_renderer_dump_transport_scpd(void);

struct device *upnp_renderer_new(const char *friendly_name,
				 const char *mac_addr,
				 const char *sn,
				 config_t *cfg);

#endif /* _UPNP_RENDERER_H */
