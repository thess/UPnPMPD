/* logging.h - Logging facility
 *
 * Copyright (C) 2007   Ivo Clarysse
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

#ifndef _LOGGING_H
#define _LOGGING_H

#if defined(DEBUG)
extern int g_debug;

enum {
    DBG_LVL0 = 0,
    DBG_LVL1 = 1,
    DBG_LVL2 = 2,
    DBG_LVL3 = 3,
    DBG_LVL4 = 4,
    DBG_LVL5 = 5
};

#define DBG_STATIC
#define DBG_PRINT(lvl, fmt, ...) do { if (g_debug >= lvl) fprintf(stderr, fmt, ##__VA_ARGS__); } while(0)
#else
#define DBG_STATIC static
#define DBG_PRINT(lvl, fmt, ...)
#endif

#if !defined(assert)
#define assert(expr)
#endif

#endif /* _LOGGING_H */
