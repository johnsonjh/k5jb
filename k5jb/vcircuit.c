/* vcircuit.c
 * Formally rose.c
 * synthetic interface to enable incoming virtual circuit operation from
 * ROSE, and maybe other schemes.  Optionally can restore PID in information
 * frames lost by earlier ROSE versions - K5JB
 * Cosmetic work 9/23/92
 */

#include <stdio.h>
#include "config.h"
#include "options.h"
#ifdef VCKT
#include "global.h"
#include "mbuf.h"
#include "iface.h"
#include "timer.h"
#include "arp.h"
#include "slip.h"
#include "ax25.h"
#include "vcircuit.h"
#include "lapb.h"
#include "cmdparse.h"
#include <ctype.h>
#include <string.h>

#define VCKTNUMCKTS	10	/* number of VCKT circuits permitted */

vcircuittab	vcircuit_table[VCKTNUMCKTS];
int vcircuit_nbr = 0;
int vcircuit_attached = FALSE;
int vcircuit_enable = FALSE;
extern int rose_bash;

#define MAX_VCIFACE 4	/* an arbitrary number */

struct interface *vcircuit_interface[MAX_VCIFACE] ;
int vcircuit_ifaces = 0;

/* test for virtual circuit connection via ax.25 */

int
check_vcircuit(axp)
register struct ax25_cb *axp ;
{

	register int i ;
	int addreq();
#ifdef SID2
	extern int ax25mbox;
	extern struct ax25_addr bbscall;
#endif

#ifdef NO_LONGER
/* this may speed up some packets, but this test may hinder some other
 * applications other than ROSE
 */

	if(axp->addr.ndigis < 2 || axp->addr.ndigis > 4)
		return(FALSE); /* can't be ROSE */
#endif

	/* this function is called by procdata() in lapb.c.  We want to divert
	 * this frame from mailbox if it is in table.  Src, dest and digi string
	 * have been reversed.
	 * First we test to see if call is for our aux mailbox call,
	 * then look for remote IP station's call. (No longer check corresponding
	 * ROSE neighbor's call.)
	 */

#ifdef SID2
	if(ax25mbox && addreq(&bbscall,&axp->addr.source))
		return(FALSE); /* we don't want to mess up packets to mailbox */
#endif

	for (i = 0 ;i<vcircuit_nbr; i++)
		if (addreq(&vcircuit_table[i].ipcall,&axp->addr.dest))
			return(TRUE);
	return(FALSE);
}

int
vcircuit_stop()
{
	return(0);	/* do nothing - The REAL iface will restore port
					 * this is only called at exit from program */
}

/* attach the VCKT interface, used only to make virtual circuit mode
 * independent of hardware port used
 */
int
vcircuit_attach(argc,argv)
int argc ;
char *argv[] ;
{                     /* call with vc attach xxxx ax0 */
							/* with kpc-4, use, vc attach xxxa kp0a */

	struct interface *iface,*if_lookup();
	int namelen;

#ifdef KPCPORT
	struct interface *kpcifaceb;
#endif

	if(vcircuit_ifaces == MAX_VCIFACE){
		printf("VC ifaces are at max\n");
		return(-1);
	}

	if (if_lookup(argv[1]) != NULLIF){
		printf("Virtual Interface %s already attached\n",argv[1]) ;
		return -1 ;
	}

	if((iface = if_lookup(argv[2])) == NULLIF){
		printf("Interface %s unknown\n",argv[2]);
		return(-1);
	}

	if((namelen = strlen(argv[1])) > 4){	/* selective inforcement */
		argv[1][4] = '\0';
		namelen = 4;
	}

#ifdef KPCPORT
	/* do simple test for kp0a or dr0a interfaces which have special
	 * requirements that the vc iface name end in 'a' and we will
	 * additionally force them to be 4 characters long - haven't tested
	 * but this _might_ work for drsi.
	 */

	if(argv[2][0] == 'k' || argv[2][0] == 'd'){
		if(vcircuit_ifaces == MAX_VCIFACE - 1){
			printf("KPC requires 2 VC ifaces, only 1 left\n");
			return(-1);
		}
		if(!(argv[1][3] == 'a' && argv[2][3] == 'a')){
			printf("KPC syntax: \"attach xxxa kp0a\"\n");
			return(-1);
		}
		kpcifaceb = iface->next;  /* temp pointer, actually points to kp0b */
		/* we first attach xxxb to kp0b so rosa and rosb will appear in iface
		chain in same order as kp0a and kp0b */
		vcircuit_interface[vcircuit_ifaces] =
			(struct interface *)calloc(1,sizeof(struct interface));
		memcpy(vcircuit_interface[vcircuit_ifaces],kpcifaceb,sizeof(struct interface));
		vcircuit_interface[vcircuit_ifaces]->name = (char *)malloc(namelen + 1);
		strcpy(vcircuit_interface[vcircuit_ifaces]->name,argv[1]);
		vcircuit_interface[vcircuit_ifaces]->name[3] = 'b';	/* crude but effective */
			/* the following is only reason for doing this whole
				iface thing */
		vcircuit_interface[vcircuit_ifaces]->flags |= CONNECT_MODE;
		vcircuit_interface[vcircuit_ifaces]->stop = vcircuit_stop;	/* not a real interface */
		vcircuit_interface[vcircuit_ifaces]->next = ifaces;	/* don't muck with asy */
		ifaces = vcircuit_interface[vcircuit_ifaces++];      /* when we exit */
	}
	/* now we take care of attaching xxxa to kp0a */
#endif

	vcircuit_interface[vcircuit_ifaces] =
			(struct interface *)calloc(1,sizeof(struct interface));
	memcpy(vcircuit_interface[vcircuit_ifaces],iface,sizeof(struct interface));
	vcircuit_interface[vcircuit_ifaces]->name = (char *)malloc(namelen + 1);
	strcpy(vcircuit_interface[vcircuit_ifaces]->name,argv[1]);	/* xxxx, or if
																		KPC, xxxa */
	vcircuit_interface[vcircuit_ifaces]->flags |= CONNECT_MODE;

	vcircuit_interface[vcircuit_ifaces]->stop = vcircuit_stop;
	vcircuit_interface[vcircuit_ifaces]->next = ifaces ;
	ifaces = vcircuit_interface[vcircuit_ifaces++];

	vcircuit_attached = TRUE;
	return 0 ;
}

/* use: vc add remote_IPcall.  Affects incoming packets.
 * arp add and route add are used to handle outgoing packets
 */

int
vcircuit_add(argc,argv)	int argc;
char *argv[];
{
	int setcall();

	if(vcircuit_nbr == VCKTNUMCKTS){
	printf("Inbound virtual circuit table full!\n");
		return(-1);
	}
	if(setcall(&vcircuit_table[vcircuit_nbr].ipcall,argv[1]) == -1){
		printf("Station's Call?\n");
		return(-1);
	}
	vcircuit_nbr++;
	vcircuit_enable = TRUE;
	if(!vcircuit_attached)
		printf("Note: VC not attached yet\n");
	return(0);

}

int pax25();
static char tmp[10];

int
vcircuit_drop(argc,argv)
int argc;
char *argv[];
{
	int i;

	if(setcall(tmp,argv[1]) == -1){
		printf("Call?\n");
		return(-1);
	}
	for(i=0;i<vcircuit_nbr;i++)
		if (addreq(&vcircuit_table[i].ipcall,tmp))
			break;

	if(i == vcircuit_nbr)
		return(-1);	/* wasn't in table */

	vcircuit_nbr--;	/* don't need to move last one in table */

	while(i<vcircuit_nbr){
		memcpy(&vcircuit_table[i].ipcall,&vcircuit_table[i+1].ipcall,AXALEN);
		i++;
	}
	if(!vcircuit_nbr)
		vcircuit_enable = FALSE;
	return(0);
}


/* "vcircuit" subcommands */

static struct cmds vcircuitcmds[] = {
	"add", vcircuit_add, 2,
	"vcircuit add <remote IP call>",
	"Add failed",

	"attach", vcircuit_attach, 3,
	"vcircuit attach <VC iface> <asy iface>",
	"Attach failed",

	"drop", vcircuit_drop, 2,
	"vcircuit drop <remote IP call>",
	"Not in table",

	NULLCHAR, NULLFP, 0,
	"vcircuit subcommands: add, drop, attach",
	NULLCHAR,
};

/* Display and/or manipulate virtual circuit table */
int
dovcircuit(argc,argv)
int argc;
char *argv[];
{
	int i, subcmd();
	if(argc < 2){
		printf("Incoming virtual circuit table:\n");
		if(!vcircuit_nbr){
			printf("(empty)\n");
			return(0);
		}
		for(i=0;i<vcircuit_nbr;i++){
			pax25(tmp,&vcircuit_table[i].ipcall);
			printf("Remote Call: %s\n",tmp);
		}
		return 0;
	}
	return subcmd(vcircuitcmds,argc,argv);
}

/* Toggle the bashing of IP PID back into ROSE frames (initially off) */
int
dorosebash(argc,argv)
int argc;
char *argv[];
{
	rose_bash = !rose_bash;
	printf("IP PID bashing in ROSE frames is %s.\n",rose_bash ?
		"enabled" : "disabled");
	return 0;
}

#endif	/* VCKT */
