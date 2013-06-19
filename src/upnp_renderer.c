/* upnp_renderer.c - UPnP renderer routines
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
#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <upnp/upnp.h>
#include <upnp/ithread.h>
#include <upnp/upnptools.h>

#include <libconfig.h>

#include "logging.h"
#include "webserver.h"
#include "upnp.h"
#include "upnp_device.h"
#include "upnp_connmgr.h"
#include "upnp_control.h"
#include "upnp_transport.h"

#include "upnp_renderer.h"

static struct service *upnp_services[] = {
	&transport_service,
	&connmgr_service,
	&control_service,
	NULL
};

static struct icon icon1 = {
        .width =        64,
        .height =       64,
        .depth =        24,
        .url =          "/upnp/upnpmpd-64x64.png",
        .mimetype =     "image/png"
};
static struct icon icon2 = {
        .width =        128,
        .height =       128,
        .depth =        24,
        .url =          "/upnp/upnpmpd-128x128.png",
        .mimetype =     "image/png"
};

static struct icon *renderer_icon[] = {
        &icon1,
        &icon2,
        NULL
};

static int upnp_renderer_init(void);

static struct device render_device = {
        .init_function          = upnp_renderer_init,
        .device_type            = "urn:schemas-upnp-org:device:MediaRenderer:1",
        .friendly_name          = PACKAGE_NAME,
        .manufacturer           = "Kitschensync",
        .manufacturer_url       = "http://www.kitschensync.com",
        .model_description      = "UPnP MPD Interface Service",
        .model_name             = "KS " PACKAGE_NAME,
        .model_number           = PACKAGE_VERSION,
        .model_url              = "http://www.kitschensync.com/upnpmpd",
        .serial_number          = "KSMPD-12345678",
        .udn                    = "uuid:1b42696d-0d62-4af2-adb8--aabbccddeeff",
        .upc                    = "",
        .presentation_url       = "/upnp/upnpmpd.html",
        .icons                  = renderer_icon,
        .services               = upnp_services
};

void upnp_renderer_dump_connmgr_scpd(void)
{
	fputs(upnp_get_scpd(&connmgr_service), stdout);
}
void upnp_renderer_dump_control_scpd(void)
{
	fputs(upnp_get_scpd(&control_service), stdout);
}
void upnp_renderer_dump_transport_scpd(void)
{
	fputs(upnp_get_scpd(&transport_service), stdout);
}

DBG_STATIC int upnp_renderer_init(void)
{
        // Handle these here
        transport_init();
        control_init();

        return connmgr_init();
}

struct device *upnp_renderer_new(const char *friendly_name,
                                 const char *mac_addr,
                                 const char *sn,
                                 config_t *cfg)
{
	char *udn;
	int k, k2;

	if (friendly_name == NULL)
    {
        // Config file override
        config_lookup_string(cfg, "friendly-name", &render_device.friendly_name);
    } else {
        // Use the one passed in (command-line)
        render_device.friendly_name = friendly_name;
    }

    // Fill in other device config
    config_lookup_string(cfg, "manufacturer", &render_device.manufacturer);
    config_lookup_string(cfg, "manufacturerURL", &render_device.manufacturer_url);
    config_lookup_string(cfg, "model-name", &render_device.model_name);
    config_lookup_string(cfg, "model-desc", &render_device.model_description);
    config_lookup_string(cfg, "model-URL", &render_device.model_url);
    config_lookup_string(cfg, "model-number", &render_device.model_number);

    // Copy template
    udn = strdup(render_device.udn);
    // merge given MAC into UUID
	for (k = 0; k < 6; k++)
	{
	    k2 = 2 * k;
	    udn[29 + k2] = *mac_addr;
	    udn[29 + k2 + 1] = *(mac_addr + 1);

	    mac_addr += 3;
	}

    // Unique ID for this device
	render_device.udn = udn;

	return &render_device;
}
