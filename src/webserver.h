/* webserver.h - Web server callback definitions
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

#ifndef _WEBSERVER_H
#define _WEBSERVER_H

extern struct UpnpVirtualDirCallbacks virtual_dir_callbacks;
extern int webserver_register_buf(const char *path, const char *contents,
				  const char *content_type);
extern int webserver_register_file(const char *path,
				   const char *content_type);

#endif /* _WEBSERVER_H */
