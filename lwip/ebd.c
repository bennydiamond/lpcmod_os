#include "lwip/stats.h"
#include "lwip/mem.h"
#include "netif/etharp.h"
#include "lwip/tcp.h"
#include "boot.h"

struct eth_addr ethaddr = { 0, 0x0d, 0xff, 0xff, 0, 0 };

void
eth_transmit (const char *d, unsigned int t, unsigned int s, const void *p);
int
eth_poll_into (char *buf, int *len);

static struct pbuf *
ebd_poll (struct netif *netif) {
    struct pbuf *p, *q;
    char *bufptr;
    char buf[1500];
    int len;

    if (!eth_poll_into (buf, &len))
        return NULL;

    p = pbuf_alloc (PBUF_LINK, len, PBUF_POOL);
    if (p != NULL) {
        bufptr = &buf[0];
        for (q = p; q != NULL; q = q->next) {
            memcpy (q->payload, bufptr, q->len);
            bufptr += q->len;
        }
    }
    else {
        printk ("Could not allocate pbufs\n");
    }
    return p;
}

static err_t
ebd_low_level_output (struct netif *netif, struct pbuf *p) {
    char buf[1500];
    char *bufptr;
    struct eth_hdr *h;
    struct pbuf *q;

    bufptr = &buf[0];
    h = (struct eth_hdr *) bufptr;
    for (q = p; q != NULL; q = q->next) {
        memcpy (bufptr, q->payload, q->len);
        bufptr += q->len;
    }
    eth_transmit (&h->dest.addr[0], ntohs (h->type), p->tot_len - 14, &buf[14]);
    return 0;   //Keep compiler happy
}

static err_t
ebd_output (struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr) {
    p = etharp_output (netif, ipaddr, p);
    if (p != NULL) {
        return ebd_low_level_output (netif, p);
    }
    else {
        printk ("b");
    }
    return ERR_OK;
}

int
ebd_wait (struct netif *netif, u16_t time) {
    unsigned long delay_ticks;
    static unsigned long start_ticks = 0;
    extern unsigned long
    currticks (void);

    delay_ticks = time * 3579;
    if (!start_ticks)
        start_ticks = currticks ();

    while (1) {
        struct eth_hdr *ethhdr;
        struct pbuf *p, *q;

        p = ebd_poll (netif);
        if (p) {
            ethhdr = p->payload;
            q = NULL;
            switch (htons (ethhdr->type)) {
                case ETHTYPE_IP:
                    q = etharp_ip_input (netif, p);
                    pbuf_header (p, -14);
                    netif->input (p, netif);
                    break;
                case ETHTYPE_ARP:
                    q = etharp_arp_input (netif, &ethaddr, p);
                    break;
                default:
                    pbuf_free (p);
                    break;
            }
            if (q != NULL) {
                ebd_low_level_output (netif, q);
                pbuf_free (q);
            }
            return 1;
        }
        else {
            unsigned long ticks = currticks () - start_ticks;
            if (ticks > delay_ticks) {
                start_ticks = 0;
                return 0;
            }
        }
    }
    return 0;   //Keep compiler happy
}

extern char forcedeth_hw_addr[6];

static err_t
ebd_init (struct netif *netif) {
    netif->hwaddr_len = 6;
    memcpy (netif->hwaddr, forcedeth_hw_addr, 6);
    memcpy (ethaddr.addr, forcedeth_hw_addr, 6);
    netif->name[0] = 'e';
    netif->name[1] = 'b';
    netif->output = ebd_output;
    netif->linkoutput = ebd_low_level_output;
    return ERR_OK;
}

int
run_lwip (unsigned char flashType) {
    struct ip_addr ipaddr, netmask, gw;
    struct netif netif;
    bool first = 1;

    mem_init ();
    memp_init ();
    pbuf_init ();
    netif_init ();
    ip_init ();
    udp_init ();
    tcp_init ();
    etharp_init ();
    printk ("\n\n            TCP/IP initialized.\n");
    netFlashOver = false;

    /*	IP4_ADDR(&gw, 192,168,99,1);
     IP4_ADDR(&ipaddr, 192,168,99,2);
     IP4_ADDR(&netmask, 255,255,255,0);
     */
    //These will be overwritten by DHCP anyway if need be.
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

    netif_add (&netif, &ipaddr, &netmask, &gw, NULL, ebd_init, ip_input);
    if (LPCmodSettings.OSsettings.useDHCP){
        dhcp_start (&netif);
    }
    else {
        //Not necessary, but polite.
        dhcp_stop (&netif);
        netif_set_addr(&netif, &ipaddr, &netmask, &gw);
        //netif_set_ipaddr (&netif, &ipaddr);
        //netif_set_netmask (&netif, &netmask);
        //netif_set_gw (&netif, &gw);
        dhcp_inform (&netif);
    }

    netif_set_default (&netif);

    httpd_init(flashType);
    int divisor = 0;
    while (!netFlashOver) {
        if (!ebd_wait (&netif, TCP_TMR_INTERVAL)) {
            //printk ("!ebd_wait");
            if (divisor++ == 60 * 4) {
                
                dhcp_coarse_tmr ();
                divisor = 0;
            }
            if(first && divisor == 10){
	        if (netif.dhcp->state != DHCP_BOUND && LPCmodSettings.OSsettings.useDHCP) {
	            printk ("\n            DHCP FAILED - Falling back to %u.%u.%u.%u",
	                ipaddr.addr & 0x000000ff,
	                (ipaddr.addr & 0x0000ff00) >> 8,
	                (ipaddr.addr & 0x00ff0000) >> 16,
	                (ipaddr.addr & 0xff000000) >> 24);
	            dhcp_stop (&netif);
	            netif_set_addr(&netif, &ipaddr, &netmask, &gw);
	        }
	        printk ("\n\n            Go to 'http://%u.%u.%u.%u' to flash your BIOS.\n",
	            ((netif.ip_addr.addr) & 0xff),
	            ((netif.ip_addr.addr) >> 8 & 0xff),
	            ((netif.ip_addr.addr) >> 16 & 0xff),
	            ((netif.ip_addr.addr) >> 24 & 0xff));
	        first = 0;
                }
            if (divisor & 1){
                dhcp_fine_tmr ();
            }
            tcp_tmr ();
            //else
            //	printk("Got packet!! \n");
        }
    }
    return 0;   //Keep compiler happy
}

