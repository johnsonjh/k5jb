#include <stdio.h>
#include "sokname.h"	/* for SOKNAME */
#include "global.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "tcp.h"
#include "cmdparse.h"

long atol();	/* K5JB correct wierd timer values we get */
int atoi();	/* while we are at it */

/* TCP connection states - reduced size of several for formatting - K5JB */
char *tcpstates[] = {
	"Closed",
	"Listen",
	"SYN sent",
	"SYN rcvd",
	"Estab.",
	"FINwait1",
	"FINwait2",
	"ClosWait",
	"Closing",
	"Last ACK",
	"Timewait"
};

/* TCP closing reasons */
char *reasons[] = {
	"Normal",
	"Reset",
	"Timeout",
	"ICMP"
};
/* TCP subcommand table */
int domss(),doirtt(),dortt(),dotcpstat(),dowindow(),dotcpkick(),dotcpreset(),
	dotcptimer();

struct cmds tcpcmds[] = {
	"irtt",		doirtt,		0,	NULLCHAR,	NULLCHAR,
	"kick",		dotcpkick,	2,	"tcp kick <tcb>",	NULLCHAR,
	"mss",		domss,		0,	NULLCHAR,	NULLCHAR,
	"reset",	dotcpreset,	2,	"tcp reset <tcb>", NULLCHAR,
	"rtt",		dortt,		3,	"tcp rtt <tcb> <val>", NULLCHAR,
	"status",	dotcpstat,	0,	NULLCHAR,	NULLCHAR,
	"timertype",	dotcptimer,	0, NULLCHAR,	NULLCHAR,
	"window",	dowindow,	0,	NULLCHAR,	NULLCHAR,
	NULLCHAR,	NULLFP,		0,
		"tcp subcommands: irtt kick mss reset rtt status timertype window",
		NULLCHAR,
};
int
dotcp(argc,argv)
int argc;
char *argv[];
{
	return subcmd(tcpcmds,argc,argv);
}

/* Eliminate a TCP connection */
static int
dotcpreset(argc,argv)
int argc;
char *argv[];
{
	register struct tcb *tcb;
	extern char notval[];

	tcb = (struct tcb *)htol(argv[1]);
	if(!tcpval(tcb)){
		printf(notval);
		return 1;
	}
	close_self(tcb,RESET);
	return 0;
}

/* Set initial round trip time for new connections */
static int
doirtt(argc,argv)
int argc;
char *argv[];
{
	if(argc < 2)
		printf("%lu\n",tcp_irtt);
	else
		tcp_irtt = (int32)atol(argv[1]);
	return 0;
}

/* Set smoothed round trip time for specified TCB */
static int
dortt(argc,argv)
int argc;
char *argv[];
{
	register struct tcb *tcb;
	extern char notval[];

	tcb = (struct tcb *)htol(argv[1]);
	if(!tcpval(tcb)){
		printf(notval);
		return 1;
	}
	tcb->srtt = (int32)atol(argv[2]);
	return 0;
}

/* Force a retransmission */
static int
dotcpkick(argc,argv)
int argc;
char *argv[];
{
	register struct tcb *tcb;
	extern char notval[];

	tcb = (struct tcb *)htol(argv[1]);
	if(kick_tcp(tcb) == -1){
		printf(notval);
		return 1;
	}
	return 0;
}

/* Set default maximum segment size */
static int
domss(argc,argv)
int argc;
char *argv[];
{
	if(argc < 2)
		printf("%u\n",tcp_mss);
	else
		tcp_mss = (int16)atoi(argv[1]);
	return 0;
}

/* Set default window size */
static int
dowindow(argc,argv)
int argc;
char *argv[];
{
	if(argc < 2)
		printf("%u\n",tcp_window);
	else
		tcp_window = (int16)atoi(argv[1]);
	return 0;
}

/* Display status of TCBs */
static int
dotcpstat(argc,argv)
int argc;
char *argv[];
{
	register struct tcb *tcb;
	extern char notval[];

	if(argc < 2){
		tstat();
	} else {
		tcb = (struct tcb *)htol(argv[1]);
		if(tcpval(tcb))
			state_tcp(tcb);
		else
			printf(notval);
	}
	return 0;
}

/* Dump TCP stats and summary of all TCBs
 *     &TCB Rcv-Q Snd-Q Local socket           Remote socket           State
 * If SOKNAME defined:
 *     1234     0     0 k5jb.okla.ampr:xxxxxxx n5owk.okla.ampr:xxxxxxx Estab.
 * Else:
 *     1234     0     0 44.78.0.2:xxxx         44.78.0.12:xxxx         Estab.
 */
static int
tstat()
{
	register int i;
	register struct tcb *tcb;
#ifdef SOKNAME
	char *puname();
#else
	char *psocket();
#endif

	printf("conout %u, conin %u, reset out %u, runt %u, chksum err %u, bdcsts %u\n",
		tcp_stat.conout,tcp_stat.conin,tcp_stat.resets,tcp_stat.runt,
		tcp_stat.checksum,tcp_stat.bdcsts);
	printf("    &TCB RcvQ SndQ Local socket            Remote socket           State\n");
	for(i=0;i<NTCB;i++){
		for(tcb=tcbs[i];tcb != NULLTCB;tcb = tcb->next){
			printf("%8lx%5u%5u ",(long)tcb,tcb->rcvcnt,tcb->sndcnt);
#ifdef SOKNAME
			printf("%-24s",puname(&tcb->conn.local));
			printf("%-24s",puname(&tcb->conn.remote));
#else
			printf("%-24s",psocket(&tcb->conn.local));
			printf("%-24s",psocket(&tcb->conn.remote));
#endif
			printf("%-8s",tcpstates[tcb->state]);
			if(tcb->state == LISTEN && (tcb->flags & CLONE))
				printf(" (S)");
			printf("\n");
		}
	}
	fflush(stdout);
	return 0;
}
/* Dump a TCP control block in detail */
static void
state_tcp(tcb)
struct tcb *tcb;
{
	int32 sent,recvd;
#ifdef SOKNAME
	char *puname();
#else
	char *psocket();
#endif

	if(tcb == NULLTCB)
		return;
	/* Compute total data sent and received; take out SYN and FIN */
	sent = tcb->snd.una - tcb->iss;	/* Acknowledged data only */
	recvd = tcb->rcv.nxt - tcb->irs;
	switch(tcb->state){
	case LISTEN:
	case SYN_SENT:		/* Nothing received or acked yet */
		sent = recvd = 0;
		break;
	case SYN_RECEIVED:
		recvd--;	/* Got SYN, no data acked yet */
		sent = 0;
		break;
	case ESTABLISHED:	/* Got and sent SYN */
	case FINWAIT1:		/* FIN not acked yet */
		sent--;
		recvd--;
		break;
	case FINWAIT2:		/* Our SYN and FIN both acked */
		sent -= 2;
		recvd--;
		break;
	case CLOSE_WAIT:	/* Got SYN and FIN, our FIN not yet acked */
	case CLOSING:
	case LAST_ACK:
		sent--;
		recvd -= 2;
		break;
	case TIME_WAIT:		/* Sent and received SYN/FIN, all acked */
		sent -= 2;
		recvd -= 2;
		break;
	}
#ifdef SOKNAME
	printf("Local: %s",puname(&tcb->conn.local));
	printf(" Remote: %s",puname(&tcb->conn.remote));
#else
	printf("Local: %s",psocket(&tcb->conn.local));
	printf(" Remote: %s",psocket(&tcb->conn.remote));
#endif
	printf(" State: %s\n",tcpstates[tcb->state]);
	printf("      Init seq    Unack     Next Resent CWind Thrsh  Wind  MSS Queue      Total\n");
	printf("Send:");
	printf("%9lx",tcb->iss);
	printf("%9lx",tcb->snd.una);
	printf("%9lx",tcb->snd.nxt);
	printf("%7lu",tcb->resent);
	printf("%6u",tcb->cwind);
	printf("%6u",tcb->ssthresh);
	printf("%6u",tcb->snd.wnd);
	printf("%5u",tcb->mss);
	printf("%6u",tcb->sndcnt);
	printf("%11lu\n",sent);

	printf("Recv:");
	printf("%9lx",tcb->irs);
	printf("         ");
	printf("%9lx",tcb->rcv.nxt);
	printf("%7lu",tcb->rerecv);
	printf("      ");
	printf("      ");
	printf("%6u",tcb->rcv.wnd);
	printf("     ");
	printf("%6u",tcb->rcvcnt);
	printf("%11lu\n",recvd);

	if(tcb->reseq != (struct reseq *)0){	/* K5JB, also below */
		register struct reseq *rp;

		printf("Reassembly queue:\n");
		for(rp = tcb->reseq;rp != (struct reseq *)0; rp = rp->next){
			printf("  seq x%lx %u bytes\n",rp->seg.seq,rp->length);
		}
	}
	if(tcb->backoff > 0)
		printf("Backoff %u ",tcb->backoff);
	if(tcb->flags & RETRAN)
		printf("Retrying ");
	switch(tcb->timer.state){
	case TIMER_STOP:
		printf("Timer stopped ");
		break;
	case TIMER_RUN:
		printf("Timer running (%ld/%ld ms)\n",
		 (int32)MSPTICK * (tcb->timer.start - tcb->timer.count),
		 (int32)MSPTICK * tcb->timer.start);
		break;
	case TIMER_EXPIRE:
		printf("Timer expired ");
	}
	printf("SRTT %ld ms Mean dev %ld ms\n",tcb->srtt,tcb->mdev);
	fflush(stdout);
}

int32 backoffcap = 1800000L/(int32)MSPTICK; /* initially 30 minutes */

/* tcp timers type - linear v exponential */
static int
dotcptimer(argc,argv)
int argc ;
char *argv[] ;
{
	extern int tcptimertype;
	int32 bkoff;

	if (argc < 2) {
		printf("Tcp timer type is %s.  Max backoff is %ld min.\n",
			tcptimertype ? "linear" : "exponential",
#ifdef UNIX
			(backoffcap * (int32)MSPTICK)/60000L);
#else
			((backoffcap * (int32)MSPTICK)/60000L) + 1);	/* white lie here */
#endif
		return 0 ;
	}

	switch (argv[1][0]) {
		case 'l':
		case 'L':
			tcptimertype = 1 ;
			break ;
		case 'e':
		case 'E':
			tcptimertype = 0 ;
			break ;
		default:
			printf("use: tcp timertype [linear|exponential] [<max backoff minutes>]\n");
			return -1 ;
	}
	if(argc == 3)	/* interpret variable as minutes */
		if((bkoff = (int32)atol(argv[2])) > 4)	/* keep it reasonable */
			backoffcap = (bkoff * 60000L)/(int32)MSPTICK;
	return 0 ;
}
