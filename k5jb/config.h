/*
 * just about everything except "MSDOS" is defined here, or in options.h.
 * and config.inc.  Makefile only has minimum to make it agree with compiler
 * used.  Some of the code that can be enabled here is incomplete or not 
 * guaranteed to work.  Suspect code is marked "CE"  caveat emptor).
 */

#ifndef GOTOPTH
		/* this is here so decisions can be made below. Any file 
		   that depends on options.h must include it for depend.out's
		   sake and configuration control.  Include options.h before
		   config.h and the compiler won't complain about later defs
		   hiding earlier ones.
		 */
#define GOTOPTH
#include "options.h"
#endif

/* Software options */
#define	SERVERS		/* Include TCP servers */
#define	TRACE		/* Include packet tracing code */
#define _TELNET		/* Telnet server - don't undef this in Unix */
#define	NSESSIONS 10	/* Number of interactive clients */
#define	TYPE		/* Include type command */
#define	FLOW		/* Enable local tty flow control */
#define _FINGER		/* Enable Finger - was in makefile - adds 2924 bytes */
#define	NETROM		/* NET/ROM network support - adds 20996 bytes */
#define SEGMENT		/* NOS type ax25 frame segmentation */

/* see options.h for commonly changed options:

	DRSI		DRSI standard driver
	KPCPORT	Kantronics KPC port switch command - N5OWK
	MULPORT 	Grapes Multiport code
	VCKT	RATS ROSE switch (and others) work-around
	AX25_HEARD A "heard" and "jheard" function
	PACKET		Sufficient FTP Corp. packet driver to do G8BPQ
*/

/* untested... */
#undef	SCREEN		/* trace screen on the Atari-ST - CE */

/* Hardware configuration */
#define	SLIP		/* Serial line IP */
#define	KISS		/* KISS TNC code, implies AX25 - adds 32306 bytes */

/* untested things */
#undef	APPLETALK	/* Appletalk interface (Macintosh) -- CE */
#undef	PC_EC		/* 3-Com 3C501 Ethernet controller -- CE */
#undef	PLUS		/* HP's Portable Plus is the platform -- CE */
#undef	NRS  		/* NET/ROM async interface -- CE */

#if defined(NRS)
#undef	NETROM
#define	NETROM		/* NRS implies NETROM */
#endif

/* shortened this to include only things I mess with */
#if (defined(NETROM) || defined(KISS) || defined(DRSI) || defined (KPCPORT) \
	|| defined(PACKET) || defined(COMBIOS))
#define	AX25		/* AX.25 subnet code - with KISS adds 32306 bytes */
#define AXMBX
#endif

/* undef AXMBX to cut 11,048 bytes and leave ax.25 with only chat session
#undef AXMBX
*/

#ifdef AXMBX
#define SID2		/* for ax25 mbox - adds 738 bytes */
#endif

/* KISS TNC, SLIP, NRS, PACKET or COMBIOS implies ASY */
#if (defined(KISS) || defined(PACKET) || defined(NRS) || defined(SLIP) \
	|| defined(SLFP) || defined(COMBIOS))
#undef	ASY
#define	ASY		/* Asynch driver code */
#endif

#ifdef PC_EC
#define	ETHER		/* Generic Ethernet code -- CE */
#endif			
	/* define ETHER if you want to try Ethernet with PACKET.  PACKET is
	   defined in options.h and has only been used recently with G8BPQ
	   switch code.  The ETHER part is pretty primitive.
	 */
