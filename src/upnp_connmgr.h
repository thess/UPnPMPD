/* upnp_connmgr.h - UPnP Connection Manager definitions
 *
 * Copyright (C) 2005   Ivo Clarysse
 * Copyright (C) 2012   Ted Hess (Kitschensync)
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

#ifndef _UPNP_CONNMGR_H
#define _UPNP_CONNMGR_H

extern struct service connmgr_service;
extern void register_mime_type(const char *mime_type);
extern int connmgr_init(void);

#endif /* _UPNP_CONNMGR_H */
