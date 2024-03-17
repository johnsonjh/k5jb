/*
 *	kpc.c
 *
 *	Driver for the Kantronics KPC-4 and KAM Dual Port TNC
 *	Ideas taken from both the asy and drsi drivers.
 *
 *	Aug 91 - Created the beast. N5OWK
 *	Sep 91 - Debugged. N5OWK + K5JB
 */

#include "options.h"

#ifdef KPCPORT
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "mbuf.h"
#include "ax25.h"
#include "slip.h"
#include "asy.h"
#include "iface.h"
#include "kiss.h"
#include "trace.h"
#if defined(MSDOS) && defined(SERIALTEST)
#include "8250.h"
#endif

void	kpc_recv(), doslip();
int	kpc0_raw(), kpc1_raw();
int	asy_stop(), ax_send(), ax_output(), kiss_ioctl();

/*
 * Attach a serial interface to the kpc-4 or KAM tnc
 * argv[0]: hardware type, must be "kpc4"
 * argv[1]: I/O address, e.g., "0x3f8"
 * argv[2]: vector, e.g., "4"
 * argv[3]: interface label, e.g., "kp0" (changed to "kp0a" and "kp0b")
 * argv[4]: receiver ring buffer size in bytes
 * argv[5]: maximum transmission unit, bytes
 * argv[6]: interface speed, e.g, "4800"
 */

int
kpc_attach(argc,argv)
int argc;
char *argv[];
{
	register struct interface *if_pca,*if_pcb;
	extern struct interface *ifaces;
	int16 dev;
	int axarp(),atoi(),asy_init(),asy_speed();

	if(nasy >= ASY_MAX)  {
		printf("Too many asy controllers\n");
		return -1;
	}

	/*
	 *	The kpc-4 and KAM have two ports on one serial line.
	 *	You multiplex between the two by setting the high
	 *	nibble of the kiss command byte.  You only need one
	 *	device (dev) per kpc-4 or KAM.
	 */

	dev = nasy++;
	axarp();

	if(mycall.call[0] == '\0'){
		printf("set mycall first\n");
		nasy--;
		return -1;
	}

	/* Create iface structures and fill in details */

	if_pca = (struct interface *)calloc(1,sizeof(struct interface));
	if_pcb = (struct interface *)calloc(1,sizeof(struct interface));

	/* Append "a" to iface associated with port 0 */

	if_pca->name = (char *)malloc((unsigned)strlen(argv[3])+2);
	strcpy(if_pca->name,argv[3]);
	strcat(if_pca->name,"a");

	/* Append "b" to iface associated with port 1 */

	if_pcb->name = (char *)malloc((unsigned)strlen(argv[3])+2);
	strcpy(if_pcb->name,argv[3]);
	strcat(if_pcb->name,"b");

	if_pca->mtu    = if_pcb->mtu    = (int16)atoi(argv[5]);

#ifndef FRAGTEST  /* Disable to test fragmentation.  There is a memory bug
						 * in the ax25 vc mode if fragmentation used - K5JB
						 */
	if(if_pca->mtu > 256)
		if_pca->mtu = if_pcb->mtu = 256;
#endif

	if_pca->dev    = if_pcb->dev    = dev;
	if_pca->recv   = if_pcb->recv   = doslip;
	if_pca->stop   = if_pcb->stop   = asy_stop;
	if_pca->ioctl  = if_pcb->ioctl  = kiss_ioctl;
	if_pca->send   = if_pcb->send   = ax_send;
	if_pca->output = if_pcb->output = ax_output;
	if_pca->raw    = kpc0_raw;
	if_pcb->raw    = kpc1_raw;
	if_pca->hwaddr = (char *)malloc(sizeof(mycall));
	if_pcb->hwaddr = (char *)malloc(sizeof(mycall));
	memcpy(if_pca->hwaddr,(char *)&mycall,sizeof(mycall));
	memcpy(if_pcb->hwaddr,(char *)&mycall,sizeof(mycall));

	slip[dev].recv = kpc_recv;

	/* Link em into the iface chain */
	if_pca->next = if_pcb;	/* a is linked to b */
	if_pcb->next = ifaces;	/* b is linked to last iface assigned */
	ifaces = if_pca;	/* last iface pointer now links to a */

	asy_init(dev,argv[1],argv[2],(unsigned)atoi(argv[4]));
	asy_speed(dev,atoi(argv[6]));
	return 0;
}

/* Send raw data packet on kiss */

static int
kpc0_raw(interface,data)
struct interface *interface;
struct mbuf *data;
{
	register struct mbuf *bp;
	int dump(),slip_raw();

	dump(interface,IF_TRACE_OUT,TRACE_AX25,data);

	/* Put type field for KISS TNC on front */
	if((bp = pushdown(data,1)) == NULLBUF){
		free_p(data);
		return(0);
	}

	bp->data[0] = KISS_DATA;
	slip_raw(interface,bp);
	return(0);
}

static int
kpc1_raw(interface,data)
struct interface *interface;
struct mbuf *data;
{
	register struct mbuf *bp;
	int slip_raw();

	dump(interface,IF_TRACE_OUT,TRACE_AX25,data);

	/* Put type field for KISS TNC on front */
	if((bp = pushdown(data,1)) == NULLBUF){
		free_p(data);
		return(0);
	}

	bp->data[0] = KISS_DATA | 0x10;
	slip_raw(interface,bp);
	return(0);
}

/*
 * Process incoming KISS TNC frame, doslip already scooped the frame
 * There's no way of telling which interface it's calling with though, so
 * you have to look it up (if_lookup).
 */

static void
kpc_recv(interface,bp)
struct interface *interface;
struct mbuf *bp;
{
	int	i;
	char	kisstype, tmp[8];
	struct interface *if_lookup();
	int ax_recv();
#if defined(MSDOS) && defined(SERIALTEST)
/* Also done in kiss.c, slip.c and nrs.c to clear error flag possibly
 * set in the async handler
 */
	extern int16 serial_err;        /* defined in ipcmd.c */
	struct fifo *fp;
	fp = &asy[interface->dev].fifo;
	if(fp->error){  /* contains 8250 line status register */
		serial_err++;
		fp->error = 0;
		free_p(bp);
		return;
	}
#endif

	kisstype = pullchar(&bp);

	/* the kpc spec says it will be non-zero for  */
	/* port 1, and zero for port 0, testing shows */
	/* that a 0x10 is OR'd with the command byte  */

	if ((kisstype & 0x0F) == KISS_DATA)  {
		/*
		 *	Conceivably there may be 10 (0 to 9) dual port
		 *	modems hooked up.  We'll be replacing last char
		 *	in name...
		 */

		strcpy(tmp, interface->name); /* make a local copy */
		i = strlen(tmp);

		if ((kisstype & 0xF0) != 0)	/* check for true, port 1 */
			/*
			 *	...and force an 'a' or 'b' depending
			 *	on high the nibble check above
			 */

			tmp[i - 1] = 'b';
		else				/* false, port 0 */
			tmp[i - 1] = 'a';

		interface = if_lookup(tmp);

		dump(interface,IF_TRACE_IN,TRACE_AX25,bp);
		ax_recv(interface,bp); /* in ax25.c */
	}else{
		extern int16 freeps;
		free_p(bp);	/* probably no good */
		freeps++;
	}
}
#endif /* KPCPORT */
