/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/init.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"
#include "lwip/dhcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/timers.h"
#include "string.h"
#include "xblast/settings/xblastSettingsDefs.h"
#include "lib/cromwell/cromString.h"
#include "httpd.h"
#include "string.h"
#include "stdlib.h"
#include <stdarg.h>

bool netFlashOver;

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'b'

typedef enum
{
	NetworkState_Idle = 0U,
	NetworkState_Init = 1,
	NetworkState_DHCPStart = 2,
	NetworkState_ServerInit = 3,
	NetworkState_ServerRunning = 4,
	NetworkState_ServerShuttingDown = 5,
	NetworkState_Cleanup = 6
}NetworkState;

static NetworkState currentNetworkState = NetworkState_Idle;
static struct ip_addr ipaddr, netmask, gw;
static struct netif *netif = NULL;
static int divisor = 0;

void
eth_transmit (const char *d, unsigned int t, unsigned int s, const void *p);
int
eth_poll_into (char *buf, int *len);

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
#if 0
struct ethernetif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
};
#endif

/* Forward declarations. */
static void  ethernetif_input(struct netif *netif);

extern char forcedeth_hw_addr[6];

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
low_level_init(struct netif *netif)
{
#if 0
  struct ethernetif *ethernetif = netif->state;
#endif

  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set MAC hardware address */
  memcpy (netif->hwaddr, forcedeth_hw_addr, ETHARP_HWADDR_LEN);

  /* maximum transfer unit */
  netif->mtu = 1500;
  
  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
 
  /* Do whatever else is needed to initialize interface. */  
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
#if 0
  struct ethernetif *ethernetif = netif->state;
#endif
  char buf[1500];
  char *bufptr;
  struct eth_hdr *h;
  struct pbuf *q;
  
#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif
  bufptr = &buf[0];
  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    //send data from(q->payload, q->len);
	memcpy (bufptr, q->payload, q->len);
	bufptr += q->len;
  }

  h = (struct eth_hdr *) buf;

  //signal that packet should be sent();
  eth_transmit (&h->dest.addr[0], ntohs (h->type), p->tot_len - sizeof(struct eth_hdr), &buf[sizeof(struct eth_hdr)]);

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif)
{
  struct pbuf *p, *q;
  char *bufptr;
  char buf[1500];
  int len;

  /* Obtain the size of the packet and put it into the "len"
     variable. */
  if (!eth_poll_into (buf, &len))
    return NULL;

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  
  if (p != NULL) {
	  
	bufptr = &buf[0];

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    for(q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
       * available data in the pbuf is given by the q->len
       * variable.
       * This does not necessarily have to be a memcpy, you can also preallocate
       * pbufs for a DMA-enabled MAC and after receiving truncate it to the
       * actually received size. In this case, ensure the tot_len member of the
       * pbuf is the sum of the chained pbuf len members.
       */
      //read data into(q->payload, q->len);
	  memcpy (q->payload, bufptr, q->len);
      bufptr += q->len;
    }
    //acknowledge that packet has been read();

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } else {
    //drop packet();
    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }

  return p;  
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
static void
ethernetif_input(struct netif *netif)
{
#if 0
  struct ethernetif *ethernetif;
#endif
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  //ethernetif = netif->state;

  /* move received packet into a new pbuf */
  p = low_level_input(netif);
  /* no packet could be read, silently ignore this */
  if (p == NULL) return;
  /* points to packet payload, which starts with an Ethernet header */
  ethhdr = p->payload;

  switch (htons(ethhdr->type)) {
  /* IP or ARP packet? */
  case ETHTYPE_IP:
  case ETHTYPE_ARP:
#if PPPOE_SUPPORT
  /* PPPoE packet? */
  case ETHTYPE_PPPOEDISC:
  case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    /* full packet send to tcpip_thread to process */
    if (netif->input(p, netif)!=ERR_OK)
     { LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
       pbuf_free(p);
       p = NULL;
     }
    break;

  default:
    pbuf_free(p);
    p = NULL;
    break;
  }
  
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
#if 0
  struct ethernetif *ethernetif;
#endif

  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if 0
  ethernetif = mem_malloc(sizeof(struct ethernetif));
  if (ethernetif == NULL) {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
    return ERR_MEM;
  }
#endif

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000); //100Mbps

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;//ebd_output
  netif->linkoutput = low_level_output;//ebd_low_level_output
  
  
  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

int
ebd_wait (u16_t time) {
    unsigned long delay_ticks;
    static unsigned long start_ticks = 0;
    extern unsigned long currticks (void);

    delay_ticks = time * 3579;
    if (start_ticks == 0)
        start_ticks = currticks ();

	unsigned long ticks = currticks () - start_ticks;
	if (ticks > delay_ticks) {
		start_ticks = 0;
		return 1;	//Delay's over
	}
	
    return 0;   //not finished
}


int
run_lwip (unsigned char flashType) {

    bool returnResult = 0;

    switch(currentNetworkState)
    {
    case NetworkState_Idle:
		debugSPIPrint("currentNetworkState == NetworkState_Idle");
    	//Init a couple of Lwip globals because they seem to assume declared pointers are set to NULL by default.
		//From tcp.c
		tcp_bound_pcbs = NULL;
		tcp_listen_pcbs.listen_pcbs = NULL;
		tcp_active_pcbs = NULL;
		tcp_tw_pcbs = NULL;
		tcp_tmp_pcb = NULL;

		//from netif.c
		netif_list = NULL;
		netif_default = NULL;

		//From udp.c
		udp_pcbs = NULL;

		divisor = 0;
		currentNetworkState = NetworkState_Init;
    	debugSPIPrint("currentNetworkState == NetworkState_Init");
    	break;
    case NetworkState_Init:
        /*mem_init ();
        memp_init ();
        pbuf_init ();
        netif_init ();
        ip_init ();
        udp_init ();
        tcp_init ();
        etharp_init ();
		sys_timeouts_init();*/
    	lwip_init();

        printk ("\n            TCP/IP initialized.\n");
		netFlashOver = false;

		netif = netif_find("eb");   //Trying to find previously configured network interface

		if(netif == NULL)
		{
			debugSPIPrint("No configured network interface found. Creating one.");
			netif = (struct netif *)malloc(sizeof(struct netif));
			//Will never be removed for entire duration of program execution so no free()...

			//These will be overwritten by DHCP anyway if need be.
			IP4_ADDR(&gw, 0,0,0,0);
			IP4_ADDR(&ipaddr, 0,0,0,0);
			IP4_ADDR(&netmask, 0,0,0,0);

			netif_add (netif, &ipaddr, &netmask, &gw, NULL, ethernetif_init, ethernet_input);
		}
		else
		{
			debugSPIPrint("Found previously configured network interface.");
		}

		if (LPCmodSettings.OSsettings.useDHCP)
		{
			//Re-run DHCP discover anyways just in case lease was revoked.
			dhcp_start (netif);

			currentNetworkState = NetworkState_DHCPStart;
			debugSPIPrint("currentNetworkState == NetworkState_DHCPStart");
		}
		else
		{
			//Not necessary, but polite.
			dhcp_stop (netif);
			IP4_ADDR(&gw, LPCmodSettings.OSsettings.staticGateway[0],
					 LPCmodSettings.OSsettings.staticGateway[1],
					 LPCmodSettings.OSsettings.staticGateway[2],
					 LPCmodSettings.OSsettings.staticGateway[3]);
			IP4_ADDR(&ipaddr, LPCmodSettings.OSsettings.staticIP[0],
					 LPCmodSettings.OSsettings.staticIP[1],
					 LPCmodSettings.OSsettings.staticIP[2],
					 LPCmodSettings.OSsettings.staticIP[3]);
			IP4_ADDR(&netmask, LPCmodSettings.OSsettings.staticMask[0],
					 LPCmodSettings.OSsettings.staticMask[1],
					 LPCmodSettings.OSsettings.staticMask[2],
					 LPCmodSettings.OSsettings.staticMask[3]);
			netif_set_addr(netif, &ipaddr, &netmask, &gw);

			dhcp_inform (netif);

			currentNetworkState = NetworkState_ServerInit;
			debugSPIPrint("currentNetworkState == NetworkState_ServerInit");
		}
		netif_set_default (netif);
    	break;
    case NetworkState_DHCPStart:
	    ethernetif_input(netif);
		
    	if (ebd_wait(TCP_TMR_INTERVAL))
    	{
			if (divisor++ == 60 * 4) //1 minute timeout before forfeiting
			{
				if (netif->dhcp->state != DHCP_BOUND)
				{
					//Not necessary, but polite.
					dhcp_stop (netif);
					IP4_ADDR(&gw, LPCmodSettings.OSsettings.staticGateway[0],
							 LPCmodSettings.OSsettings.staticGateway[1],
							 LPCmodSettings.OSsettings.staticGateway[2],
							 LPCmodSettings.OSsettings.staticGateway[3]);
					IP4_ADDR(&ipaddr, LPCmodSettings.OSsettings.staticIP[0],
							 LPCmodSettings.OSsettings.staticIP[1],
							 LPCmodSettings.OSsettings.staticIP[2],
							 LPCmodSettings.OSsettings.staticIP[3]);
					IP4_ADDR(&netmask, LPCmodSettings.OSsettings.staticMask[0],
							 LPCmodSettings.OSsettings.staticMask[1],
							 LPCmodSettings.OSsettings.staticMask[2],
							 LPCmodSettings.OSsettings.staticMask[3]);
					netif_set_addr(netif, &ipaddr, &netmask, &gw);

					dhcp_inform (netif);
				}
				divisor=0;
				currentNetworkState = NetworkState_ServerInit;
				debugSPIPrint("currentNetworkState == NetworkState_ServerInit");
			}
		}

    	if(netif->dhcp->state == DHCP_BOUND)
    	{
    		currentNetworkState = NetworkState_ServerInit;
			debugSPIPrint("currentNetworkState == NetworkState_ServerInit");
    	}
    	break;
    case NetworkState_ServerInit:
        printk ("\n\n            Go to 'http://%u.%u.%u.%u' to flash your BIOS.\n",
            ((netif->ip_addr.addr) & 0xff),
            ((netif->ip_addr.addr) >> 8 & 0xff),
            ((netif->ip_addr.addr) >> 16 & 0xff),
            ((netif->ip_addr.addr) >> 24 & 0xff));

		httpd_init(flashType);

		currentNetworkState = NetworkState_ServerRunning;
    	debugSPIPrint("currentNetworkState == NetworkState_ServerRunning");
    	break;
    case NetworkState_ServerRunning:
	    ethernetif_input(netif);

    	if(netFlashOver == true)
    	{
    		currentNetworkState = NetworkState_ServerShuttingDown;
			debugSPIPrint("currentNetworkState == NetworkState_ServerShuttingDown");
    	}
    	break;
    case NetworkState_ServerShuttingDown:
    	dhcp_stop(netif);

    	currentNetworkState = NetworkState_Cleanup;
    	debugSPIPrint("currentNetworkState == NetworkState_Cleanup");
    	break;
    case NetworkState_Cleanup:
    	returnResult = 1;

    	currentNetworkState = NetworkState_Idle;
    	break;
    }

    sys_check_timeouts();

    return returnResult;   //Keep compiler happy
}
