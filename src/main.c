/* main.c - Main program routines
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

#if defined(__i386__) && defined(DEBUG)
#include <execinfo.h>
#include <signal.h>
#include <ucontext.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <glib.h>

#include <upnp/ithread.h>
#include <upnp/upnp.h>

#include <libconfig.h>

#include "logging.h"
#include "output_mpd.h"
#include "upnp.h"
#include "upnp_device.h"
#include "upnp_renderer.h"

static gboolean show_version = FALSE;
static gboolean show_devicedesc = FALSE;
static gboolean show_connmgr_scpd = FALSE;
static gboolean show_control_scpd = FALSE;
static gboolean show_transport_scpd = FALSE;
static gboolean run_as_daemon = FALSE;
static gchar *if_name[] = { NULL, NULL };
static gchar *friendly_name = NULL;
static gchar gIF_NAME[LINE_SIZE] = { '\0' };
static gchar gIF_IPV4[22] = { '\0' };
static gchar *config_file = NULL;
static gchar *serial = NULL;

// Config file data
#define CFG_FILE_NAME   "/etc/upnpmpd.conf"
config_t upnpmpd_cfg;

#if defined(DEBUG)
// Current debug level
int g_debug = 0;
#endif

#if defined(__i386__) && defined(DEBUG)
/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
typedef struct _sig_ucontext {
    unsigned long     uc_flags;
    struct ucontext   *uc_link;
    stack_t           uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t          uc_sigmask;
} sig_ucontext_t;

static void crit_err_hdlr(int sig_num, siginfo_t * info, void * ucontext)
{
    void *array[50];
    void *caller_address;
    char **messages;
    int size, i;
    sig_ucontext_t *uc;

    uc = (sig_ucontext_t *)ucontext;

    /* Get the address at the time the signal was raised from the EIP (x86) */
    caller_address = (void *) uc->uc_mcontext.eip;

    fprintf(stderr, "!!! Signal %d (%s), address is %p from %p\n",
                    sig_num, strsignal(sig_num), info->si_addr, (void *)caller_address);

    size = backtrace(array, 50);

    /* overwrite sigaction with caller's address */
    array[1] = caller_address;

    messages = backtrace_symbols(array, size);

    /* skip first stack frame (points here) */
    for (i = 1; i < size && messages != NULL; ++i)
    {
        fprintf(stderr, "[bt]: (%d) %s\n", i, messages[i]);
    }

    free(messages);

    exit(EXIT_FAILURE);
}
#endif

#define MAX_INTERFACES  4
static int getIfInfo(const char *IfName)
{
    char szBuffer[MAX_INTERFACES * sizeof(struct ifreq)];
    struct ifconf ifConf;
    struct ifreq ifReq;
    int i;
    int LocalSock;
    //FILE* inet6_procfd;
    //struct in6_addr v6_addr;
    //int if_idx;
    //char addr6[8][5];
    //char buf[65];       // INET6_ADDRSTRLEN
    int ifname_found = 0;
    int valid_addr_found = 0;

    // Copy interface name, if it was provided.
    if( IfName != NULL ) {
        if( strlen(IfName) > sizeof(gIF_NAME) )
            return -1;

        strncpy( gIF_NAME, IfName, sizeof(gIF_NAME) );
        ifname_found = 1;
    }

    // Create an unbound datagram socket to do the SIOCGIFADDR ioctl on.
    if( ( LocalSock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) < 0 ) {
        printf("Can't create addrlist socket\n");
        return -1;
    }

    // Get the interface configuration information...
    ifConf.ifc_len = sizeof szBuffer;
    ifConf.ifc_ifcu.ifcu_buf = ( caddr_t ) szBuffer;

    if( ioctl( LocalSock, SIOCGIFCONF, &ifConf ) < 0 ) {
        printf("DiscoverInterfaces: SIOCGIFCONF returned error\n");
        return -1;
    }

    // Cycle through the list of interfaces looking for IP addresses.
    for( i = 0; i < ifConf.ifc_len ; ) {
        struct ifreq *pifReq =
            ( struct ifreq * )( ( caddr_t ) ifConf.ifc_req + i );
        i += sizeof *pifReq;

        // See if this is the sort of interface we want to deal with.
        strcpy( ifReq.ifr_name, pifReq->ifr_name );
        if( ioctl( LocalSock, SIOCGIFFLAGS, &ifReq ) < 0 ) {
            printf("Can't get interface flags for %s:\n", ifReq.ifr_name);
            return -1;
        }

        // Skip LOOPBACK interfaces, DOWN interfaces and interfaces that
        // don't support MULTICAST.
        if( ( ifReq.ifr_flags & IFF_LOOPBACK )
            || ( !( ifReq.ifr_flags & IFF_UP ) )
            || ( !( ifReq.ifr_flags & IFF_MULTICAST ) ) ) {
            continue;
        }

        if( ifname_found == 0 ) {
            // We have found a valid interface name. Keep it.
            strncpy( gIF_NAME, pifReq->ifr_name, sizeof(gIF_NAME) );
            ifname_found = 1;
        } else {
            if( strncmp( gIF_NAME, pifReq->ifr_name, sizeof(gIF_NAME) ) != 0 ) {
                // This is not the interface we're looking for.
                continue;
            }
        }

        // Check address family.
        if( pifReq->ifr_addr.sa_family == AF_INET ) {
            // Copy interface name, IPv4 address and interface index.
            strncpy( gIF_NAME, pifReq->ifr_name, sizeof(gIF_NAME) );
            inet_ntop(AF_INET, &((struct sockaddr_in*)&pifReq->ifr_addr)->sin_addr, gIF_IPV4, sizeof(gIF_IPV4));
            //gIF_INDEX = if_nametoindex(gIF_NAME);

            valid_addr_found = 1;
            break;
        } else {
            // Address is not IPv4
            ifname_found = 0;
        }
    }
    close( LocalSock );

    // Failed to find a valid interface, or valid address.
    if( ifname_found == 0  || valid_addr_found == 0 ) {
        printf("Failed to find an adapter with valid IP addresses for use.\n");
        return -1;
    }

#if 0
    // Try to get the IPv6 address for the same interface
    // from "/proc/net/if_inet6", if possible.
    inet6_procfd = fopen( "/proc/net/if_inet6", "r" );
    if( inet6_procfd != NULL ) {
        while( fscanf(inet6_procfd,
                "%4s%4s%4s%4s%4s%4s%4s%4s %02x %*02x %*02x %*02x %*20s\n",
                addr6[0],addr6[1],addr6[2],addr6[3],
                addr6[4],addr6[5],addr6[6],addr6[7], &if_idx) != EOF) {
            // Get same interface as IPv4 address retrieved.
            if( gIF_INDEX == if_idx ) {
                snprintf(buf, sizeof(buf), "%s:%s:%s:%s:%s:%s:%s:%s",
                    addr6[0],addr6[1],addr6[2],addr6[3],
                    addr6[4],addr6[5],addr6[6],addr6[7]);
                // Validate formed address and check for link-local.
                if( inet_pton(AF_INET6, buf, &v6_addr) > 0 &&
                    IN6_IS_ADDR_LINKLOCAL(&v6_addr) ) {
                    // Got valid IPv6 link-local adddress.
                    strncpy( gIF_IPV6, buf, sizeof(gIF_IPV6) );
                    break;
                }
            }
        }
        fclose( inet6_procfd );
    }
#endif
    return 0;
}

/* Generic UPnPMPD options */
static GOptionEntry option_entries[] = {
	{ "version", 0, 0, G_OPTION_ARG_NONE, &show_version,
	  "Output version information and exit", NULL },
    { "config", 'C', 0, G_OPTION_ARG_STRING, &config_file,
      "Read option file from path (Default: /etc/upnpmpd.conf)", NULL },
	{ "if-name", 'I', 0, G_OPTION_ARG_STRING, &if_name[0],
	  "Interface name on which to listen", NULL },
	{ "friendly-name", 'F', 0, G_OPTION_ARG_STRING, &friendly_name,
	  "Friendly name to advertise", NULL },
    { "daemon", 'B', 0, G_OPTION_ARG_NONE, &run_as_daemon,
      "Detach process to background", NULL },
	{ "dump-devicedesc", 0, 0, G_OPTION_ARG_NONE, &show_devicedesc,
	  "Dump device descriptor XML and exit", NULL },
	{ "dump-connmgr-scpd", 0, 0, G_OPTION_ARG_NONE, &show_connmgr_scpd,
	  "Dump Connection Manager service description XML and exit", NULL },
	{ "dump-control-scpd", 0, 0, G_OPTION_ARG_NONE, &show_control_scpd,
	  "Dump Rendering Control service description XML and exit", NULL },
	{ "dump-transport-scpd", 0, 0, G_OPTION_ARG_NONE, &show_transport_scpd,
	  "Dump A/V Transport service description XML and exit", NULL },
#if defined(DEBUG)
    { "debuglevel", 0, 0, G_OPTION_ARG_INT, &g_debug,
      "Set debug trace level (0 := lowest)", NULL },
#endif
	{ NULL }
};

static void do_show_version(void)
{
	puts( PACKAGE_STRING "\n"
        "This is free software. "
            "You may redistribute copies of it under the terms of\n"
		"the GNU General Public License "
            "<http://www.gnu.org/licenses/gpl.html>.\n"
		"There is NO WARRANTY, to the extent permitted by law."
	);
}

static int process_cmdline(int argc, char **argv)
{
	GOptionContext *ctx;
	GError *err = NULL;
	int rc;

	ctx = g_option_context_new("- UPnPMPD");
	g_option_context_add_main_entries(ctx, option_entries, NULL);

	rc = output_mpd_add_options(ctx);
	if (rc != 0)
		return -1;

	if (!g_option_context_parse(ctx, &argc, &argv, &err))
	{
		g_print("Failed to initialize: %s\n", err->message);
		g_error_free(err);
		return -1;
	}

	return 0;
}

static char *ethers[] = { "wlan0", "eth0", "eth1", NULL };
#define ether_tmpl  "/sys/class/net/%s/address"

int main(int argc, char **argv)
{
	int rc, cnt;
	struct device *upnp_renderer;
    char mac_addr[18];
    char eth_name[sizeof(ether_tmpl) + 10];
    char **eth;
    FILE *fh = NULL;

#if defined(__i386__) && defined(DEBUG)
    struct sigaction sigact;

    sigact.sa_sigaction = crit_err_hdlr;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL) != 0)
    {
        fprintf(stderr, "error setting signal handler for %d (%s)\n",
                        SIGSEGV, strsignal(SIGSEGV));

        exit(EXIT_FAILURE);
    }
#endif
    // Init glib threads
	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

    // Parse args early (may set debug-level)
	rc = process_cmdline(argc, argv);
	if (rc != 0) {
		exit(EXIT_FAILURE);
    }

    // Some commands are valid only if interactive
    if (!run_as_daemon)
    {
        // Info commenads (non-daemon)
        if (show_version) {
            do_show_version();
            return EXIT_SUCCESS;
        }

        if (show_connmgr_scpd) {
            upnp_renderer_dump_connmgr_scpd();
            return EXIT_SUCCESS;
        }

        if (show_control_scpd) {
            upnp_renderer_dump_control_scpd();
            return EXIT_SUCCESS;
        }

        if (show_transport_scpd) {
            upnp_renderer_dump_transport_scpd();
            return EXIT_SUCCESS;
        }
    } else {
        if (show_version || show_control_scpd || show_connmgr_scpd ||
            show_transport_scpd || show_devicedesc)
        {
            // Info commands not allow in daemon mode
            fprintf(stderr, "Info options incompatible with daemon\n");
            exit(EXIT_FAILURE);
        }

        rc = daemon(0, 0);
        if (rc != 0)
        {
            fprintf(stderr, "Failed to detach process\n");
            exit(EXIT_FAILURE);
        }
    }

    // Read and parse config file, OK if none
    if (config_file == NULL)
        config_file = CFG_FILE_NAME;


    config_init(&upnpmpd_cfg);
    rc = config_read_file(&upnpmpd_cfg, (config_file) ? config_file : CFG_FILE_NAME);
    if (rc != CONFIG_TRUE)
    {
        if (config_error_line(&upnpmpd_cfg) == 0)
        {
            printf("Config not found (%s), continuing...\n", config_file);
        } else {
            printf ("Error parsing config file: %s\nLine %d: %s", config_file,
                    config_error_line(&upnpmpd_cfg), config_error_text(&upnpmpd_cfg));
            exit(EXIT_FAILURE);
        }
    }

    // Use bound interface if given
    if (if_name[0])
        eth = &if_name[0];
    else
        eth = &ethers[0];

    while (*eth)
    {
        sprintf(eth_name, ether_tmpl, *eth);
        // Obtain MAC address from interface
        fh = fopen(eth_name, "r");
        if (fh)
            break;
        // Try next
        eth++;
    }

    // anything?
    if (fh == NULL)
    {
        fprintf(stderr, "No usable network interface found\n");
        exit(EXIT_FAILURE);
    }
    // Read 6 groups of 3 chars
    cnt = fread(mac_addr, 3, 6, fh);
    fclose(fh);

    // Valid looking MAC?
    if (cnt != 6)
    {
        fprintf(stderr, "Bad MAC address %d\n", cnt);
        exit(EXIT_FAILURE);
    }

    mac_addr[17] = '\0';
    DBG_PRINT(DBG_LVL2, "Found MAC at %s: %s\n", eth_name, mac_addr);

	upnp_renderer = upnp_renderer_new(friendly_name, mac_addr, serial, &upnpmpd_cfg);
	if (upnp_renderer == NULL)
		exit(EXIT_FAILURE);

	if (show_devicedesc) {
		fputs(upnp_get_device_desc(upnp_renderer), stdout);
		return EXIT_SUCCESS;
	}

    // Connect to MPD
	rc = output_mpd_init(&upnpmpd_cfg);
	if (rc != 0)
		exit(EXIT_FAILURE);

    // Create UPnP device and broadcast
    rc = getIfInfo(if_name[0]);
    if (rc != 0)
        exit(EXIT_FAILURE);

	rc = upnp_device_init(upnp_renderer, gIF_IPV4);
	if (rc != 0)
		exit(EXIT_FAILURE);

    printf("UPnPMPD ready at IP: %s\n", gIF_IPV4);

    // Run main event loop
    // Process glib evnets
	output_loop();

	return EXIT_SUCCESS;
}
