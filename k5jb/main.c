/* Main metwork program - provides both client and server functions
 * See readme.src for modification notes - K5JB
 * #define CPUSTAT to measure loop performance - adds 768 bytes
 */

#define HOSTNAMELEN 64

#include <stdio.h>
#include <string.h>
#ifdef UNIX
#include <sys/types.h>
#endif
#include <time.h>

#ifdef __TURBOC__
#include <io.h>
#include <fcntl.h>
#endif
#include "options.h"
#include "config.h"
#include "sokname.h"
#include "global.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "icmp.h"
#include "iface.h"
#include "ip.h"
#include "tcp.h"
#ifdef AX25
#include "ax25.h"
#include "lapb.h"
#endif
#ifdef NETROM
#include "netrom.h"
#include "nr4.h"
#endif
#include "ftp.h"
#include "telnet.h"
#include "remote.h"
#ifdef _FINGER
#include "finger.h"
#endif
#include "session.h"
#include "cmdparse.h"

#ifdef  ASY
#include "asy.h"
#include "slip.h"
#endif

#ifdef  NRS
#include "nrs.h"
#endif

#ifdef  SLFP
#include "slfp.h"
#endif

#ifdef UNIX             /* BSD or SYS5 */
#include "unix.h"
#include "unixopt.h"
#endif

#ifdef AMIGA
#include "amiga.h"
#endif

#ifdef MAC
#include "mac.h"
#endif

#ifdef MSDOS
#include "asy.h"
#endif

#ifdef  ATARI_ST
#include "st.h"

#ifdef  LATTICE
long _MNEED = 100000L;          /* Reserve RAM for subshell... */
long _32K = 0x8000;             /* For GST Linker (Don't ask me! -- hyc) */
#endif

#ifdef  MWC
long    _stksize = 16384L;      /* Fixed stack size... -- hyc */
#endif
#endif  /* ATARI_ST */

#ifdef  TRACE
#include "trace.h"
/* Dummy structure for loopback tracing */
struct interface loopback = { NULLIF, "loopback" };
#endif

/* various file and path names used by fileinit() */

extern char *homedir,*startup,*userfile,*hosts,*spool,*mailspool,
		*mailqdir,*mailqueue,*routeqdir,*alias,*helpbox,*public;
#ifdef _FINGER
extern char *fingersuf,*fingerpath;
#endif

#ifdef MULPORT
extern char *mdigilist,*mexlist;
#endif

#ifdef	UNIX
extern char *netexe;
#endif

extern struct interface *ifaces;
void version();
extern struct mbuf *loopq;
extern FILE *trfp;
extern char trname[];

int Keyhit = -1;
int mode;
int exitval;	/* used by doexit() */

static char *logname;
static FILE *logfp;

char badhost[] = "Unknown host %s\n";
char hostname[HOSTNAMELEN];
unsigned nsessions = NSESSIONS;
int32 resolve();
char *strncpy();
int16 lport = 1024;
char prompt[] = "net> ";
char nospace[] = "No space!!\n";        /* Generic malloc fail message */

#ifdef  SYS5
#include <signal.h>
int background = 0;
int backgrd = 0;	/* used by shell layer management - K5JB */
extern int reportquit_flag;
void report_quit();
int io_active = 0;
#endif

#if ((!defined(MSDOS) && !defined(ATARI_ST)) || defined(PC9801))        /* PC/ST uses F-10 key always */
static char escape = 0x1d;      /* default escape character is ^] */
#endif

/* Enter command mode */
int
cmdmode()
{
	void cooked();

	if(mode != CMD_MODE){
		mode = CMD_MODE;
		cooked();
		printf(prompt);
		fflush(stdout);
	}
	return 0;
}

int	/* removed static to make this the only sane exit from program */
doexit()	/* left declaration as int to match cmd list */
{
	void iostop(),exit();
#ifdef PLUS
/*
 * set: cursor to block, attributes off, keyboard to HP mode,
 *      transmit functions off, use HP fonts
 */
	printf(/* "\033*dK"   cursor to block         */
		"\033&d@"       /* display attributes off  */
		/* "\033&k0\\"     KBD to HP mode          */
		/* "\033&s0A"      transmit functions off  */
		"\033[10m");     /* use HP fonts           */
#endif

#ifdef TRACE
	if (trfp != stdout)
	  fclose(trfp);
#endif
#ifdef  SYS5
	if (!background)
		iostop();	/* this function will exit() */
#else			/* it is also a signal handler */
	iostop();
#endif

	exit(exitval);
}

static
dohostname(argc,argv)
int argc;
char *argv[];
{

	if(argc < 2)
		printf("%s\n",hostname);
	else
		strncpy(hostname,argv[1],HOSTNAMELEN);
	return 0;
}

static
int
dolog(argc,argv)
int argc;
char *argv[];
{
	char *make_path();
	void free();

	if(argc < 2){
		if(logname != NULLCHAR)
			printf("Log: %s. log off stops.\n",logname);
		else
			printf("Log off. log <fn> starts.\n");
		return 0;
	}
	if(logname != NULLCHAR){
		free(logname);
		logname = NULLCHAR;
	}
	if(strcmp(argv[1],"off") == 0)
		return 0;
	if(argv[1][0] == '/' || argv[1][0] == '\\')
		logname = make_path("",argv[1],0);
	else
		logname = make_path(homedir,argv[1],1);
	return 0;
}

static
int
dohelp()
{
	extern struct cmds cmds[];
	register struct cmds *cmdp;
	int i;

	printf("Main commands:");	/* first command is "" and will do \n */
	for(i=0,cmdp = cmds;cmdp->name != NULLCHAR;cmdp++,i++){
		printf("%-15s",cmdp->name);
		if(!(i % 5))
			printf("\n");
	}
	printf("\n");
	return 0;
}

static
int
dohelp1(argc,argv)
int argc;
char *argv[];
{
	char *make_path();
	void free();
	char *pp;
	int i;
	char helpname[16];	/* should be big enough even for Unix */
	char *defaulthelp = "net";
	int dohelpfile();

	if(argc < 2)
		pp = defaulthelp;
	else
		pp = argv[1];

	for(i=0;i<11;i++){
		helpname[i] = pp[i];
		if(helpname[i] == '\0')
			break;
	}
	helpname[i] = '\0';	/* to be sure */
	strcat(helpname,".hlp");
	pp = make_path(homedir,helpname,1);
	if(pp != NULLCHAR){
		dohelpfile(pp,0);    /* ignore return value */
		free(pp);
	}
	return 0;
}

static  	/* primitive way to peek at our mail, page at a time */
int
domail(argc,argv)
int argc;
char *argv[];
{
	char *make_path();
	void free();
	int atoi();
	char *pp;
	char *mfname;
	int i;
	char hostmail[16];	/* should be big enough even for Unix */

	if(argc < 2)
		mfname = hostname;
	else
		mfname = argv[1];
	for(i=0;i<11;i++){
		if(mfname[i] == '.' || mfname[i] == '\0')
			break;
		hostmail[i] = mfname[i];
	}
	hostmail[i] = '\0';
	strcat(hostmail,".txt");
	pp = make_path(mailspool,hostmail,1);
	if(argc == 3)
		i = 22 * atoi(argv[2]);
		else
			i = 0;
	if(pp != NULLCHAR){
		if(dohelpfile(pp,i) == -1)
			printf("Or there is no mail\n");
		free(pp);
	}
	return 0;
}

int
dohelpfile(file,jump)
char *file;
int jump;
{
#define HFLINE 120	/* arbitrary, but this will keep
								cycles down on long lines */
	extern int Keyhit;
	FILE *hfile;
	extern char escape;
	extern char nospace[];
	char *hbuf;
	int i;
	void keep_things_going(),check_kbd(),free();

	Keyhit = -1;
	if((hbuf = (char *)malloc(sizeof(char) * HFLINE)) == NULLCHAR){
		perror(nospace);
		return(0);
	}
	if((hfile = fopen(file,"r")) == NULLFILE){
		printf("Can't find %s\n",file);
		fflush(stdout);
		free(hbuf);
		return(-1);
	}
	if(jump)
		for(i=0;i<jump;i++)
			if(fgets(hbuf,HFLINE,hfile) == NULLCHAR){
				fclose(hfile);
				free(hbuf);
				return(0);
			}

	for(;;){
		for(i=0;i<22;i++){
			if(fgets(hbuf,HFLINE,hfile) == NULLCHAR || hbuf[0] == 0x1a){
				Keyhit = 0; /* fake an escape */
				break;
			}
			printf(hbuf);	/* already has EOL appended */
			fflush(stdout);
			keep_things_going();
		}
		if(Keyhit == 0)
			break;
		printf("\nHit any key to continue, ");

#ifdef UNIX
		printf("escape key to quit...");
#else
		printf("F10 to quit...");
#endif
		fflush(stdout);
		do{
			keep_things_going();
#ifdef UNIX
			Keyhit = kbread();
#else
			check_kbd();
#endif
		}while(Keyhit == -1);
#ifdef UNIX
		if(Keyhit == escape)
#else
		if(Keyhit == -2)	/* the F10 key */
#endif
			break;
		Keyhit = -1;
		printf("\n\n");
		fflush(stdout);
	}	/* for(ever) while there is a file and no escape */
	printf("\n");
	fflush(stdout);
	fclose(hfile);
	free(hbuf);
	return(0);
}

#ifdef _TELNET
int
doecho(argc,argv)
int argc;
char *argv[];
{
	extern int refuse_echo;

	if(argc < 2){
		if(refuse_echo)
			printf("Refuse\n");
		else
			printf("Accept\n");
	} else {
		if(argv[1][0] == 'r')
			refuse_echo = 1;
		else if(argv[1][0] == 'a')
			refuse_echo = 0;
		else
			return -1;
	}
	return 0;
}
#endif /* _TELNET */

#ifdef _TELNET
/* set for unix end of line for remote echo mode telnet */
doeol(argc,argv)
int argc;
char *argv[];
{
	extern int unix_line_mode;

	if(argc < 2){
		if(unix_line_mode)
			printf("Unix\n");
		else
			printf("Standard\n");
	} else {
		if(strcmp(argv[1],"unix") == 0)
			unix_line_mode = 1;
		else if(strcmp(argv[1],"standard") == 0)
			unix_line_mode = 0;
		else {
			return -1;
		}
	}
	return 0;
}
#endif /* _TELNET */

int subcmd();

/* Attach an interface
 * Syntax: attach <hw type> <I/O address> <vector> <mode> <label> <bufsize>
		<speed>
 */
doattach(argc,argv)
int argc;
char *argv[];
{
	extern struct cmds attab[];

	return subcmd(attab,argc,argv);
}

#ifdef MSDOS
unsigned heapsize;	/* used only on pc with grabcore (see pc.c) */
#endif

#ifdef CPUSTAT
/* cpu performance statistics - This was added to instrument code that
 * is either on a busy or slow machine, or using a processor intensive
 * driver like the DRSI thing.  It will show if the program is running
 * fast enough to service interface chains effectively.
 * syntax: cpu [time], where time, in seconds, would change from the
 * default of 600 second intervals for cycle accumulation.
 * This will not be documented in the user documentation.  Joe, K5JB
 */
time_t cycles[7];	/* cycle accumulators, ten minutes each */
time_t next_update;
int last_cycle;
time_t update_interval = 600L;

void
cycle(first)
int first;
{
	int i;
	time_t curr_time;

	/* first count will be short on account of time() granularity */
	if(first){
		for(i=0;i<7;i++)
			cycles[i] = 0L;
		time(&next_update);
		next_update += update_interval;
		return;
	}

	/* every 16th time through, check time */
	if((cycles[0]++ & 0x0F) == 0x0F){
		time(&curr_time);
		if(curr_time < next_update)
			return;
	}else
		return;

	/* Do it this way, rather than absolute time, to detect dormant periods */
	next_update += update_interval;
	for(i=6;i>0;i--)
		cycles[i] = cycles[i-1] == 16 ? 0 : cycles[i-1]; /* "16" when dormant */
	cycles[0] = 0L;
}

int
report_cycles(argc,argv)
int argc;
char **argv;
{
	time_t now;
	int i;
	extern time_t next_update, update_interval;
	extern time_t cycles[];
#ifdef COMBIOS	/* this is temporary */
extern long combios_stat[];
#endif
	if(argc == 2 && (i = atoi(argv[1])) != 0){
		update_interval = (time_t)i;
		cycle(1);
#ifdef COMBIOS	/* this is temporary */
combios_stat[0] = combios_stat[1] = 0;
#endif
		printf("Update interval changed to %ld sec.\n",update_interval);
		return 0;
	}
	now = update_interval/60L; /* use this temporarily */
	printf("Cycles per %ld %s. through main loop, least to most recent:\n",
		now > 0 ? now : update_interval, now > 0 ? "min" : "sec");
	for(i=6;i>=1;i--)	/* don't report current accumulater */
		printf("%8ld\n",cycles[i]);
	time(&now);
	if(next_update > now)	/* prevent showing when late */
		printf("%d seconds before next update\n",(int)(next_update - now));
#ifdef COMBIOS	/* more temporary */
printf("Combios 1 = %lu, Combios 2 - %lu\n",combios_stat[0],combios_stat[1]);
#endif
	return 0;	/* to be consistent with other commands */
}
#endif	/* CPUSTAT */

/* Manipulate I/O device parameters */
doparam(argc,argv)
int argc;
char *argv[];
{
	register struct interface *ifp;

	for(ifp=ifaces;ifp != NULLIF;ifp = ifp->next){
		if(strcmp(argv[1],ifp->name) == 0)
			break;
	}
	if(ifp == NULLIF){
		printf("Interface \"%s\" unknown\n",argv[1]);
		return 1;
	}
	if(ifp->ioctl == NULLFP){
					 printf("Not avail.\n");
		return 1;
	}

	/* Pass rest of args to device-specific code */
	return (*ifp->ioctl)(ifp,argc-2,argv+2);
}


/* Command lookup and branch table */
int go(),doax25(),doconnect(),dotelnet(),doclose(),doreset(),dotcp(),
	dotrace(),doescape(),doroute(),doip(),doarp(),dosession(),doftp(),
	dostart(),dostop(),dosmtp(),doudp(),dodump(),dorecord(),doupload(),
	dokick(),domode(),doshell(),dodir(),
#ifdef AXMBX
	dombox(),
#endif
#ifdef FORWARD
	doforward(),
#endif
#ifdef MULPORT
	mulport(),
#endif
	docd(),doatstat(),doping(),doremote();

#ifdef VCKT
int dovcircuit();
int dorosebash();
int rose_bash = 0;
#endif
#ifdef NETROM
int donetrom();
#ifdef NRS
int donrstat();
#endif
#endif
#ifndef UNIX
int memstat();
#endif

#ifdef ETHER
int doetherstat();
#endif
#ifdef DRSI
int dodrstat();
#endif
#ifdef EAGLE
int doegstat();
#endif
#ifdef HAPN
int dohapnstat();
#endif
#ifdef _FINGER
int dofinger();
#endif
#ifdef SOKNAME
int issok = 1;
int 
flipsok()	/* int to be consistent with rest of commands */
{
	issok = !issok;
	printf("Socket names are %s\n",issok ? "on" : "off");
	return(0);
}
#endif
struct cmds cmds[] = {
	/* The "go" command must be first */
	"",             go,             0, NULLCHAR,    NULLCHAR,
#ifdef SHELL
	"!",            doshell,        0, NULLCHAR,    NULLCHAR,
#endif
#if     (MAC && APPLETALK)
	"applestat",    doatstat,       0,      NULLCHAR,       NULLCHAR,
#endif
#if     (defined(AX25) || defined(ETHER) || defined(APPLETALK)) /* K5JB */
	"arp",          doarp,          0, NULLCHAR,    NULLCHAR,
#endif
#ifdef  AX25
	"ax25",         doax25,         0, NULLCHAR,    NULLCHAR,
#endif
	"attach",       doattach,       2,
		"attach <hardware> <hw specific options>", NULLCHAR,
/* This one is out of alpabetical order to allow abbreviation to "c" */
#ifdef  AX25
	"connect",      doconnect,      3,"connect <interface> <callsign> [digipeaters]",
		NULLCHAR,
#endif
	"cd",           docd,           0, NULLCHAR,    NULLCHAR,
	"close",        doclose,        0, NULLCHAR,    NULLCHAR,
#ifdef CPUSTAT
	"cpustat",	report_cycles,	0, NULLCHAR, NULLCHAR,
#endif
#ifdef AX25
	"disconnect",   doclose,        0, NULLCHAR,    NULLCHAR,
#endif
	"dir",          dodir,          0, NULLCHAR,    NULLCHAR,
#ifdef	DRSI
	"drsistat",		 dodrstat,       0, NULLCHAR,    NULLCHAR,
#endif
#ifdef  EAGLE
	"eaglestat",    doegstat,       0, NULLCHAR,    NULLCHAR,
#endif
#ifdef _TELNET
	"echo",         doecho,         0, NULLCHAR,    "echo [refuse|accept]",
	"eol",          doeol,          0, NULLCHAR,
		"eol options: unix, standard",
#endif
#if ((!defined(MSDOS) && !defined(ATARI_ST)) || defined(PC9801))
	"escape",       doescape,       0, NULLCHAR,    NULLCHAR,
#endif
#ifdef  PC_EC
	"etherstat",    doetherstat,    0, NULLCHAR,    NULLCHAR,
#endif  /* PC_EC */
	"exit",         doexit,         0, NULLCHAR,    NULLCHAR,
#ifdef _FINGER
	"finger",       dofinger,       0, NULLCHAR, NULLCHAR,
#endif
#ifdef FORWARD
	"forward",      doforward,      0, NULLCHAR,    NULLCHAR,
#endif
	"ftp",          doftp,          2, "ftp <address>",     NULLCHAR,
#ifdef HAPN
	"hapnstat",     dohapnstat,     0, NULLCHAR,    NULLCHAR,
#endif
	"help",         dohelp1,        0, NULLCHAR,    NULLCHAR,
	"hostname",     dohostname,     0, NULLCHAR,    NULLCHAR,
	"ip",           doip,           0, NULLCHAR,    NULLCHAR,
	"kick",         dokick,         0, NULLCHAR,    NULLCHAR,
	"log",          dolog,          0, NULLCHAR,    NULLCHAR,
#ifndef UNIX
	"memstat",      memstat,        0, NULLCHAR,    NULLCHAR,
#endif
	"mail",         domail,         0, NULLCHAR,    NULLCHAR,
#ifdef  AX25
#ifdef AXMBX
	"mbox",         dombox,         0, NULLCHAR,    NULLCHAR,
#endif
	"mode",         domode,         2, "mode <interface>",  NULLCHAR,
#ifdef MULPORT
	"mulport",      mulport,        2, "mulport <on|off>",  NULLCHAR,
#endif
#ifdef  NETROM
	"netrom",       donetrom,       0, NULLCHAR,    NULLCHAR,
#ifdef  NRS
	"nrstat",       donrstat,       0, NULLCHAR,    NULLCHAR,
#endif
#endif /* NETROM */
#endif /* AX25 */
	"param",        doparam,        2, "param <interface>", NULLCHAR,
	"ping",         doping,         0, NULLCHAR,    NULLCHAR,
#ifdef UNIX /* BSD or SYS5 */
	"pwd",          docd,           0, NULLCHAR,    NULLCHAR,
#endif
	"record",       dorecord,       0, NULLCHAR,    NULLCHAR,
	"remote",       doremote,       4, "remote <address> <port> <command>",
		NULLCHAR,
	"reset",        doreset,        0, NULLCHAR,    NULLCHAR,
	"route",        doroute,        0, NULLCHAR,    NULLCHAR,
   /* out of order cuz route (ro) is used more often */
#if defined(AX25) && defined(VCKT)
	"rosebash",     dorosebash,     0, NULLCHAR, NULLCHAR,
#endif
	"session",      dosession,      0, NULLCHAR,    NULLCHAR,
#ifdef SHELL
	"shell",        doshell,        0, NULLCHAR,    NULLCHAR,
#endif
	"smtp",         dosmtp,         0, NULLCHAR,    NULLCHAR,
#ifdef SOKNAME
	"sokname",      flipsok,        0, NULLCHAR,    NULLCHAR,
#endif
#ifdef  SERVERS
	"start",        dostart,        2, "start <servername>",NULLCHAR,
	"stop",         dostop,         2, "stop <servername>", NULLCHAR,
#endif
	"tcp",          dotcp,          0, NULLCHAR,    NULLCHAR,
#ifdef _TELNET
	"telnet",       dotelnet,       2, "telnet <address>",  NULLCHAR,
#endif
#ifdef  TRACE
	"trace",        dotrace,        0, NULLCHAR,    NULLCHAR,
#endif
	"udp",          doudp,          0, NULLCHAR,    NULLCHAR,
#if defined(_TELNET) || defined(AX25)
	"upload",       doupload,       0, NULLCHAR,    NULLCHAR,
#endif
#if defined(AX25) && defined(VCKT)
	"vcircuit",         dovcircuit,         0, NULLCHAR, NULLCHAR,
#endif
	"?",            dohelp,         0, NULLCHAR,    NULLCHAR,
	NULLCHAR,       NULLFP,         0,
		"Unknown command; type \"?\" for list",   NULLCHAR
};

#ifdef  SERVERS
/* "start" and "stop" subcommands */
int dis1(),echo1(),ftp1(),smtp1(),tn1(),rem1();

#if defined(UNIX) && defined(TELUNIX)
int tnix1();
#endif

#ifdef _FINGER
int finger1();
#endif

static struct cmds startcmds[] = {
	"discard",      dis1,           0, NULLCHAR, NULLCHAR,
	"echo",         echo1,          0, NULLCHAR, NULLCHAR,
#ifdef _FINGER
	"finger",       finger1,        0, NULLCHAR, NULLCHAR,
#endif
	"ftp",          ftp1,           0, NULLCHAR, NULLCHAR,
	"smtp",         smtp1,          0, NULLCHAR, NULLCHAR,
#ifdef _TELNET
	"telnet",       tn1,            0, NULLCHAR, NULLCHAR,
#endif
#if defined(UNIX) && defined(TELUNIX)
	"telunix",      tnix1,          0, NULLCHAR,
	"Could not start telunix - no pty?",
#endif
	"remote",       rem1,           0, NULLCHAR, NULLCHAR,
	NULLCHAR,       NULLFP,         0,
#ifdef UNIX
#ifdef TELUNIX
#ifdef _FINGER
		"start options: discard, echo, finger, ftp, remote, smtp, telnet, telunix", NULLCHAR
#else
		"start options: discard, echo, ftp, remote, smtp, telnet, telunix", NULLCHAR
#endif
#else /* TELUNIX */
#ifdef _FINGER
		"start options: discard, echo, finger, ftp, remote, smtp, telnet", NULLCHAR
#else
		"start options: discard, echo, ftp, remote, smtp, telnet", NULLCHAR
#endif
#endif /* TELUNIX */
#else /* UNIX */
#ifdef _FINGER
		"start options: discard, echo, finger, ftp, remote, smtp, telnet", NULLCHAR
#else
		"start options: discard, echo, ftp, remote, smtp, telnet", NULLCHAR
#endif
#endif /* UNIX */
};

int ftp_stop(),smtp_stop(),echo_stop(),dis_stop(),tn_stop();
int dis0(),echo0(),ftp0(),smtp0(),tn0(),rem0();

#if defined(UNIX) && defined(TELUNIX)
int tnix0();
#endif

#ifdef _FINGER
int finger0();
#endif

static struct cmds stopcmds[] = {
	"discard",      dis0,           0, NULLCHAR, NULLCHAR,
	"echo",         echo0,          0, NULLCHAR, NULLCHAR,
#ifdef _FINGER
	"finger",       finger0,        0, NULLCHAR, NULLCHAR,
#endif
	"ftp",          ftp0,           0, NULLCHAR, NULLCHAR,
	"smtp",         smtp0,          0, NULLCHAR, NULLCHAR,
#ifdef _TELNET
	"telnet",       tn0,            0, NULLCHAR, NULLCHAR,
#endif
#if defined(UNIX) && defined(TELUNIX)
	"telunix",      tnix0,          0, NULLCHAR,
	"Stop telunix failed, no server running",
#endif
	"remote",       rem0,           0, NULLCHAR, NULLCHAR,
	NULLCHAR,       NULLFP,         0,
#ifdef UNIX
#ifdef TELUNIX
#ifdef _FINGER
		"stop options: discard, echo, finger, ftp, remote, smtp, telnet, telunix", NULLCHAR
#else
		"stop options: discard, echo, ftp, remote, smtp, telnet, telunix", NULLCHAR
#endif /* _FINGER */
#else /* TELUNIX */
#ifdef _FINGER
		"stop options: discard, echo, finger, ftp, remote, smtp, telnet", NULLCHAR
#else
		"stop options: discard, echo, ftp, remote, smtp, telnet", NULLCHAR
#endif /* _FINGER */
#endif /* TELUNIX */
#else /* UNIX */
#ifdef _FINGER
		"stop options: discard, echo, finger, ftp, remote, smtp, telnet", NULLCHAR
#else
		"stop options: discard, echo, ftp, remote, smtp, telnet", NULLCHAR
#endif /* _FINGER */
#endif /* UNIX */
};
#endif /* SERVERS */

#ifdef PLUS
typedef unsigned char byte;
byte model;                      /* 0 = p plus, 1 = p, 2 = pc */
#endif

char
*make_path(path_ptr,file_ptr,slash)
char *path_ptr;
char *file_ptr;
int slash;
{
	char *cp, *malloc();

	if ((cp = malloc(strlen(path_ptr) + strlen(file_ptr) + 2)) == NULLCHAR)
		perror(nospace);
	else {
		sprintf(cp, "%s%s%s", path_ptr,slash ? "/" : "",file_ptr);
		return(cp);
	}
	return(NULLCHAR);
}

/* found idea for this in sys5unix.c, but it wasn't completed - K5JB */
void
fileinit(cmdarg)
char *cmdarg;
{
	char *ep;
	extern char *getenv();

	if((ep = getenv("NETHOME")) == NULLCHAR)
		if((ep = getenv("HOME")) == NULLCHAR)
			ep = homedir;
	if(ep != homedir)
		homedir = make_path(ep,"",0);

	/* Replace each of the file name strings with the complete path */
	if(cmdarg == NULLCHAR)
		startup = make_path(ep,startup,1);
	else
		startup = make_path(ep,cmdarg,1);

	userfile = make_path(ep,userfile,1);
	hosts = make_path(ep,hosts,1);
	alias = make_path(ep,alias,1);
	helpbox = make_path(ep,helpbox,1);
#ifdef _FINGER
	fingerpath = make_path(ep,fingerpath,1);
#endif
#ifdef MULPORT
	mdigilist = make_path(ep,mdigilist,1); /* GRAPES mulport code */
	mexlist = make_path(ep,mexlist,1);
#endif
#ifdef UNIX
	netexe = make_path(ep,"net",1);
#endif
	if((ep = getenv("PUBLIC")) != NULLCHAR)
		public = make_path(ep,"",0);
	if((ep = getenv("NETSPOOL")) != NULLCHAR)
		spool = make_path(ep,"",0);

	mailspool = make_path(spool,mailspool,1);
	mailqdir = make_path(spool,mailqdir,1);
	mailqueue = make_path(spool,mailqueue,1);
	routeqdir = make_path(spool,routeqdir,1);

}

/* Standard commands called from main */

/* If SOKNAME is defined, log messages of the form:
 * Mon Aug 26 14:32:17 1991 k5jb.okla.ampr:1003 open FTP
 * else:
 * Mon Aug 26 14:32:17 1991 44.78.0.2:1003 open FTP
 */

/*VARARGS2*/
void
log(tcb,fmt,arg1,arg2,arg3,arg4)
struct tcb *tcb;
char *fmt;
int arg1,arg2,arg3,arg4;
{
	time_t t,time();
	void rip();

	char *cp, *psocket();
#ifdef SOKNAME
	char *puname();
#endif

	if(logname == NULLCHAR)
		return;
	/* My Unix machine doesn't mind the "t" but some might */
#ifdef MSDOS
	if((logfp = fopen(logname,"a+t")) == NULLFILE)	/* can happen */
		return;
#else
	if((logfp = fopen(logname,"a+")) == NULLFILE)
		return;
#endif
	time(&t);
	cp = ctime(&t);
	cp[19] = '\0';	/* knock off the year, omit day below */
#ifdef AXMBX
	if(tcb == (struct tcb *)0)	/* from the mailbox */
		fprintf(logfp,"%s AX25 Mailbox - ",cp+4);
#endif
	else
#ifdef SOKNAME
		fprintf(logfp,"%s %s - ",cp+4,puname(&tcb->conn.remote));
#else
		fprintf(logfp,"%s %s - ",cp+4,psocket(&tcb->conn.remote));
#endif
	fprintf(logfp,fmt,arg1,arg2,arg3,arg4);
	fprintf(logfp,"\n");
	fflush(logfp);
	fclose(logfp);
}

void
genlog(who,stuff)
char *who;
char *stuff;
{
extern FILE *logfp;
extern char *logname;
char *cp;
time_t t;

	if(logname == NULLCHAR)
		return;
#ifdef MSDOS
	if((logfp = fopen(logname,"a+t")) == NULLFILE)
		return;
#else
	if((logfp = fopen(logname,"a+")) == NULLFILE)
		return;
#endif
	time(&t);
	cp = ctime(&t);
	cp[19] = '\0';	/* shorten a bit */
	fprintf(logfp,"%s %s - %s\n",cp+4,who,stuff);
	fclose(logfp);
}

/* Configuration-dependent code */

/* List of supported hardware devices */
int asy_attach(),pc_attach(),at_attach();
#ifdef NETROM
int nr_attach();
#endif

#ifdef MSDOS
#ifdef DRSI
int dr_attach();
#endif
#ifdef  EAGLE
int eg_attach();
#endif
#ifdef  HAPN
int hapn_attach();
#endif
#ifdef  PC_EC
int ec_attach();
#endif
#ifdef COMBIOS
int combios_attach();
#endif
#endif /* MSDOS */

#ifdef KPCPORT
int kpc_attach();
#endif
#ifdef  PACKET
int pk_attach();
#endif

struct cmds attab[] = {
#if defined(PC_EC) && defined(MSDOS)
	/* 3-Com Ethernet interface */
	"3c500", ec_attach, 7,
	"attach 3c500 <address> <vector> arpa <label> <buffers> <mtu>",
	"Could not attach 3c500",
#endif
#ifdef  ASY
	/* Ordinary PC asynchronous adaptor */
	"asy", asy_attach, 8,
#ifdef  UNIX
#if	(defined(SLFP) && defined(NRS))
		  "attach asy 0 <ttyname> slip|ax25|nrs|slfp <label> 0 <mtu> <speed>",
#else
#ifdef NRS
		  "attach asy 0 <ttyname> slip|ax25|nrs <label> 0 <mtu> <speed>",
#else
		  "attach asy 0 <ttyname> slip|ax25 <label> 0 <mtu> <speed>",
#endif  /* NRS */
#endif	/* SLFP && NRS */
#else  /* UNIX */
#if	(defined(SLFP) && defined(NRS))
		  "attach asy <address> <vector> slip|ax25|nrs|slfp <label> <buffers> <mtu> <speed>",
#else
#ifdef NRS
		  "attach asy <address> <vector> slip|ax25|nrs <label> <buffers> <mtu> <speed>",
#else
		  "attach asy <address> <vector> slip|ax25 <label> <buffers> <mtu> <speed>",
#endif  /* NRS */
#endif	/* SLFP && NRS */
#endif /* UNIX */
		"Could not attach asy",
#endif /* ASY */
#if defined(PC100) && defined(MSDOS)
	/* PACCOMM PC-100 8530 HDLC adaptor */
	"pc100", pc_attach, 8,
		"attach pc100 <address> <vector> ax25 <label> <buffers> <mtu> <speed>",
		"Could not attach pc100",
#endif
#if defined(DRSI) && defined(MSDOS)
	"drsi", dr_attach, 8,
		"attach drsi <address> <vector> ax25 dr0|dr1 <buffers> <mtu> <speed>",
		"Could not attach DRSI",
#endif
#ifdef KPCPORT
	"kpc4", kpc_attach, 7,
#ifdef UNIX
		"attach kpc4 0 <ttyname> <label> 0 <mtu> <speed>",
#else
		"attach kpc4 <address> <vector> kp0 <buffers> <mtu> <speed>",
#endif
		"Could not attach kpc-4",
#endif

#if defined(COMBIOS) && defined(MSDOS)
	"combios", combios_attach, 7,
		"attach combios <com#> ax25 <label> <buffers> <mtu> <speed>",
		"Couldn't attach combios",
#endif

#if defined(EAGLE) && defined(MSDOS)
	/* EAGLE RS-232C 8530 HDLC adaptor */
	"eagle", eg_attach, 8,
		"attach eagle <address> <vector> ax25 <label> <buffers> <mtu> <speed>",
		"Could not attach eagle",
#endif
#if defined(HAPN) && defined(MSDOS)
	/* Hamilton Area Packet Radio (HAPN) 8273 HDLC adaptor */
	"hapn", hapn_attach, 8,
		"attach hapn <address> <vector> ax25 <label> <rx bufsize> <mtu> csma|full",
		"Could not attach hapn",
#endif
#ifdef  APPLETALK
	/* Macintosh AppleTalk */
	"0", at_attach, 7,
		"attach 0 <protocol type> <device> arpa <label> <rx bufsize> <mtu>",
		"Could not attach Appletalk",
#endif
#ifdef NETROM
	/* fake netrom interface */
	"netrom", nr_attach, 1,
		"attach netrom",
		"Could not attach netrom",
#endif
#ifdef  PACKET
	/* FTP Software's packet driver spec */
	"packet", pk_attach, 4,
		"attach packet <int#> <label> <buffers> <mtu>",
		"Could not attach packet driver",
#endif
	NULLCHAR, NULLFP, 0,  "Unknown device",  NULLCHAR
};

/* Protocol tracing function pointers */
#ifdef  TRACE
int ax25_dump(),ether_dump(),ip_dump(),at_dump(),slfp_dump();

int (*tracef[])() = {	/* must be in same order as in trace.h */
#ifdef  AX25
	ax25_dump,
#else
	NULLFP,
#endif

#ifdef  ETHER
	ether_dump,
#else
	NULLFP,
#endif

	ip_dump,

#ifdef  APPLETALK
	at_dump,
#else
	NULLFP,
#endif

#ifdef  SLFP
	slfp_dump,
#else
	NULLFP,
#endif

	NULLFP	/* not used, just hate to see trailing "," in init list */
};
#else /* TRACE */

int (*tracef[])() = { NULLFP }; /* No tracing at all */

/* the real dump() is defined in trace.c */

void
dump(interface,direction,type,bp)
struct interface *interface;
int direction;
unsigned type;
struct mbuf *bp;
{
}

#endif /* TRACE */

#ifdef  ASY

/* Attach a serial interface to the system
 * argv[0]: hardware type, must be "asy"
 * argv[1]: I/O address, e.g., "0x3f8"
 * argv[2]: vector, e.g., "4"
 * argv[3]: mode, may be:
 *          "slip" (point-to-point SLIP)
 *          "ax25" (AX.25 frame format in SLIP for raw TNC)
 *          "nrs" (NET/ROM format serial protocol)
 *          "slfp" (point-to-point SLFP, as used by the Merit Network and MIT
 * argv[4]: interface label, e.g., "sl0"
 * argv[5]: receiver ring buffer size in bytes
 * argv[6]: maximum transmission unit, bytes
 * argv[7]: interface speed, e.g, "9600"
 * argv[8]: optional ax.25 callsign (NRS only)
 *          optional command string for modem (SLFP only)
 */
int
asy_attach(argc,argv)
int argc;
char *argv[];
{
	register struct interface *if_asy;
	extern struct interface *ifaces;
	int16 dev;
	int mode,asy_init(),asy_send(),asy_ioctl(),asy_stop(),ax_send(),ax_output();
	int kiss_raw(),kiss_ioctl(),slip_send(),slip_raw(),atoi();
	void kiss_recv(),doslip(),slip_recv(),asy_speed(),free();

#ifdef  SLFP
	int doslfp(),slfp_raw(),slfp_send(),slfp_recv(),slfp_init();
#endif

#ifdef  AX25
#ifdef NRS
	struct ax25_addr addr ;
#endif
	void axarp();
#endif
	int ax_send(),ax_output(),nrs_raw(),asy_ioctl();
	void nrs_recv();

	if(nasy >= ASY_MAX){
		printf("Too many asynch controllers\n");
		return -1;
	}
	if(strcmp(argv[3],"slip") == 0)
		mode = SLIP_MODE;
#ifdef  AX25
	else if(strcmp(argv[3],"ax25") == 0)
		mode = AX25_MODE;
#endif
#ifdef  NRS
	else if(strcmp(argv[3],"nrs") == 0)
		mode = NRS_MODE;
#endif
#ifdef  SLFP
	else if(strcmp(argv[3],"slfp") == 0)
		mode = SLFP_MODE;
#endif
	else {
		printf("Mode %s unknown for interface %s\n",
			argv[3],argv[4]);
		return(-1);
	}

	dev = nasy++;

	/* Create interface structure and fill in details */
	if_asy = (struct interface *)calloc(1,sizeof(struct interface));
	if_asy->name = malloc((unsigned)strlen(argv[4])+1);
	strcpy(if_asy->name,argv[4]);
	if_asy->mtu = (int16)atoi(argv[6]);
#ifndef FRAGTEST	/* Disable to test fragmentation.  There is a memory bug
						 * in the ax25 vc mode if fragmentation used - K5JB
						 */
	if(if_asy->mtu > 256)
		if_asy->mtu = 256;
#endif
	if_asy->dev = dev;
	if_asy->recv = doslip;
	if_asy->stop = asy_stop;

	switch(mode){
#ifdef  SLIP
	case SLIP_MODE:
		if_asy->ioctl = asy_ioctl;
		if_asy->send = slip_send;
		if_asy->output = NULLFP;        /* ARP isn't used */
		if_asy->raw = slip_raw;
		if_asy->flags = 0;
		slip[dev].recv = slip_recv;
		break;
#endif
#ifdef  AX25
	case AX25_MODE:  /* Set up a SLIP link to use AX.25 */
		axarp();
		if(mycall.call[0] == '\0'){
			printf("set mycall first\n");
			free(if_asy->name);
			free((char *)if_asy);
			nasy--;
			return -1;
		}
		if_asy->ioctl = kiss_ioctl;
		if_asy->send = ax_send;
		if_asy->output = ax_output;
		if_asy->raw = kiss_raw;
		if(if_asy->hwaddr == NULLCHAR)
			if_asy->hwaddr = malloc(sizeof(mycall));
		memcpy(if_asy->hwaddr,(char *)&mycall,sizeof(mycall));
		slip[dev].recv = kiss_recv;
		break;
#endif /* AX25 */
#ifdef  NRS
	case NRS_MODE: /* Set up a net/rom serial interface */
		if(argc < 9){
			/* no call supplied? */
			if(mycall.call[0] == '\0'){
				/* try to use default */
				printf("set mycall first or specify in attach statement\n");
				return -1;
			} else
				addr = mycall;
		} else {
			/* callsign supplied on attach line */
			if(setcall(&addr,argv[8]) == -1){
				printf ("bad callsign on attach line\n");
				free(if_asy->name);
				free((char *)if_asy);
				nasy--;
				return -1;
			}
		}
		if_asy->recv = nrs_recv;
		if_asy->ioctl = asy_ioctl;
		if_asy->send = ax_send;
		if_asy->output = ax_output;
		if_asy->raw = nrs_raw;
		if(if_asy->hwaddr == NULLCHAR)
			if_asy->hwaddr = malloc(sizeof(addr));
		memcpy(if_asy->hwaddr,(char *)&addr,sizeof(addr));
		nrs[dev].iface = if_asy;
		break;
#endif /* NRS */
#ifdef  SLFP
	case SLFP_MODE:
		if_asy->ioctl = asy_ioctl;
		if_asy->send = slfp_send;
		if_asy->recv = doslfp;
		if_asy->output = NULLFP;        /* ARP isn't used */
		if_asy->raw = slfp_raw;
		if_asy->flags = 0;
		slfp[dev].recv = slfp_recv;
		break;
#endif /* SLFP */
	}
	if_asy->next = ifaces;
	ifaces = if_asy;
	asy_init(dev,argv[1],argv[2],(unsigned)atoi(argv[5]));
	asy_speed(dev,atoi(argv[7]));
#ifdef  SLFP
	if(mode == SLFP_MODE)
		if(slfp_init(if_asy, argc>7?argv[8]:NULLCHAR) == -1) {
			printf("Request for IP address failed.\n");
			asy_stop(if_asy);
			ifaces = if_asy->next;
			free(if_asy->name);
			free((char *)if_asy);
			nasy--;
			return -1;
	    }
#endif /* SLFP */
	return 0;
}
#endif /* ASY */

#ifdef NOTUSED
#ifndef NETROM
#ifdef  AX25
struct ax25_addr nr_nodebc;
#endif
nr_route(bp)
struct mbuf *bp;
{
	free_p(bp);
}

nr_nodercv(bp)
{
	free_p(bp);
}
#endif /* NETROM */
#endif /* NOTUSED */


/* Display or set IP interface control flags */
domode(argc,argv)
int argc;
char *argv[];
{
	register struct interface *ifp;

	for(ifp=ifaces;ifp != NULLIF;ifp = ifp->next){
		if(strcmp(argv[1],ifp->name) == 0)
			break;
	}
	if(ifp == NULLIF){
		printf("Interface \"%s\" unknown\n",argv[1]);
		return 1;
	}
	if(argc < 3){
		printf("%s: %s\n",ifp->name,
		 (ifp->flags & CONNECT_MODE) ? "VC mode" : "Datagram mode");
		return 0;
	}
	switch(argv[2][0]){
	case 'v':
	case 'c':
	case 'V':
	case 'C':
		ifp->flags = CONNECT_MODE;
		break;
	case 'd':
	case 'D':
		ifp->flags = DATAGRAM_MODE;
		break;
	default:
		printf("Usage: %s [vc | datagram]\n",argv[0]);
		return 1;
	}
	return 0;
}

#ifdef SERVERS
dostart(argc,argv)
int argc;
char *argv[];
{
	return subcmd(startcmds,argc,argv);
}
dostop(argc,argv)
int argc;
char *argv[];
{
	return subcmd(stopcmds,argc,argv);
}
#endif /* SERVERS */

#ifdef  TRACE
/* Display the trace flags for a particular interface */
static void
showtrace(ifp)
	register struct interface *ifp;
{
	if(ifp == NULLIF)
		return;
	printf("%s:",ifp->name);
	if(ifp->trace & (IF_TRACE_IN | IF_TRACE_OUT)){
		if(ifp->trace & IF_TRACE_IN)
			printf(" input");
		if(ifp->trace & IF_TRACE_OUT)
			printf(" output");

		if(ifp->trace & IF_TRACE_HEX)
			printf(" (Hex/ASCII dump)");
		else if(ifp->trace & IF_TRACE_ASCII)
			printf(" (ASCII dump)");
		else
			printf(" (headers only)");
		printf("\n");
	} else
		printf(" tracing off\n");
	fflush(stdout);
}

static
int
dotrace(argc,argv)
int argc;
char *argv[];
{
	extern int notraceall;  /* trace only in command mode? */
	struct interface *ifp;
	int htoi();

	if(argc < 2){
		printf("trace mode is %s\n", (notraceall ? "cmdmode" : "allmode"));
		printf("trace to %s\n",trfp == stdout? "console" : trname);
		showtrace(&loopback);
		for(ifp = ifaces; ifp != NULLIF; ifp = ifp->next)
			showtrace(ifp);
		return 0;
	}
	if(strcmp("to",argv[1]) == 0){
		if(argc >= 3){
			if(trfp != stdout)
			        fclose(trfp);
			if(strncmp(argv[2],"con",3) == 0)
			        trfp = stdout;
			else {
#ifdef MSDOS    /* K5JB - fix tracefile treatment of \n */
				if((trfp = fopen(argv[2],"at")) == NULLFILE)
#else
				if((trfp = fopen(argv[2],"a")) == NULLFILE)
#endif
				{
					printf("%s: cannot open\n",argv[2]);
					trfp = stdout;
					return 1;
				}
			}
			strcpy(trname,argv[2]);
		} else {
			printf("trace to %s\n",trfp == stdout? "console" : trname);
		}
		return 0;
	}
	if(strcmp("loopback",argv[1]) == 0)
		ifp = &loopback;
	else if (strcmp("cmdmode", argv[1]) == 0) {
		notraceall = 1;
		return 0;
	} else if (strcmp("allmode", argv[1]) == 0) {
		notraceall = 0;
		return 0;
	} else
		for(ifp = ifaces; ifp != NULLIF; ifp = ifp->next)
			if(strcmp(ifp->name,argv[1]) == 0)
				break;

	if(ifp == NULLIF){
		printf("Interface %s unknown\n",argv[1]);
		return 1;
	}
	if(argc >= 3)
		ifp->trace = htoi(argv[2]);

	showtrace(ifp);
	return 0;
}

#endif /* TRACE */

#if ((!defined(MSDOS) && !defined(ATARI_ST)) || defined(PC9801))
static
int
doescape(argc,argv)
int argc;
char *argv[];
{
	if(argc < 2)
		printf("0x%x\n",escape);
	else
		escape = *argv[1];
	return 0;
}
#endif /* MSDOS */

static
doremote(argc,argv)
int argc;
char *argv[];
{
	struct socket fsock,lsock;
	struct mbuf *bp;
	void send_udp();

	lsock.address = ip_addr;
	fsock.address = resolve(argv[1]);
	lsock.port = fsock.port = (int16)atoi(argv[2]);
	bp = alloc_mbuf(1);
	if(strcmp(argv[3],"reset") == 0)
		*bp->data = SYS_RESET;	/* only if enabled in pc.c */
							/* in Unix, this will exit(2) */
#ifdef NOTUSED    /* crap, cost 88 bytes to do this */
	else if(strcmp(argv[3],"restart") == 0)
		*bp->data = SYS_RESTART;	/* will exit(1) */
#endif
	else if(strcmp(argv[3],"exit") == 0)
		*bp->data = SYS_EXIT;		/* will exit(1) which will distinguish */
	else {                        /* from a normal exit */
		printf("Unknown command %s\n",argv[3]);
		return 1;
	}
	bp->cnt = 1;
	send_udp(&lsock,&fsock,0,0,bp,0,0,0);
	return 0;
}

void
keep_things_going()
{
	void check_time(), ip_recv(),ntohip();
	struct interface *ifp;
	struct mbuf *bp;
#if defined(MSDOS) && !defined(PLUS)
	void giveup();
#endif
#if defined(UNIX) && defined(TELUNIX)
	void tnix_scan();
#endif
#ifdef TRACE
	void dump();
#endif

	/* Service the loopback queue */
	if((bp = dequeue(&loopq)) != NULLBUF){
		struct ip ip;
#ifdef  TRACE
		dump(&loopback,IF_TRACE_IN,TRACE_IP,bp);
#endif
		/* Extract IP header */
		ntohip(&ip,&bp);
		ip_recv(&ip,bp,0);
	}
	/* Service the interfaces */
#ifdef  SYS5
	do {
	io_active = 0;
#endif
	for(ifp = ifaces; ifp != NULLIF; ifp = ifp->next){
		if(ifp->recv != NULLVFP)
			(*ifp->recv)(ifp);
	}
#ifdef  SYS5
	} while(io_active);
#endif

#ifdef  XOBBS
	/* service the W2XO PBBS code */
	axchk();
#endif



	/* Service the clock if it has ticked */
	check_time();

#if defined(MSDOS) && !defined(PLUS)
	/* Tell DoubleDos or Desqview to let the other task run for awhile.
	 * If if neither are active, this is a no-op
	 */
	giveup();
#endif /* MSDOS */

#if defined(UNIX) && defined(TELUNIX)
	tnix_scan();	/* this is what eihalt() calls */
#endif
}

	/* Process any keyboard input - removed from main to make accessable */
	/* by other processes - K5JB */

void
check_kbd()
{
	char *ttybuf;
	int c;
	int16 cnt;
	int ttydriv(),cmdparse(),kbread();
	extern int Keyhit;
#ifdef  FLOW
	extern int ttyflow;
#endif


#ifdef  SYS5
	if(reportquit_flag)	/* set by quit signal and report is deferred */
		report_quit();		/* until now */
	if(backgrd)		/* is running in background of shl */
		return;

	while((background == 0) && ((c = kbread()) != -1))
#else
	while((c = kbread()) != -1)
#endif
	{
#if     (defined(MSDOS) || defined(ATARI_ST))
		/* c == -2 means the command escape key (F10) */
		Keyhit = c;	/* a global we use for haktc */
		if(c == -2){
			if(mode != CMD_MODE){
				printf("\n");
				cmdmode();
			}
			continue;
		}
#endif
#if defined(SYS5) || defined(_OSK)
		if(c == escape && escape != 0){
			if(mode != CMD_MODE){
				printf("\n");
				cmdmode();
			}
			continue;
		}
#endif   /* SYS5 or _OSK */

#ifndef FLOW
		if ((cnt = ttydriv(c, &ttybuf)) == 0)
			continue;
#else
		cnt = ttydriv(c, &ttybuf);
		if (ttyflow && (mode != CMD_MODE))
			go();           /* display pending chars */
		if (cnt == 0)
			continue;
#endif  /* FLOW */
#if     (!defined(MSDOS) && !defined(ATARI_ST))
		if((ttybuf[0] == escape) && (escape != 0)) {
			if(mode != CMD_MODE){
				printf("\n");
				cmdmode();
			}
			continue;
		}
#endif
		switch(mode){
			case CMD_MODE:
				(void)cmdparse(cmds,ttybuf);
				fflush(stdout);
			break;
			case CONV_MODE:
#if ((!defined(MSDOS) && !defined(ATARI_ST)) || defined(PC9801))
				if(ttybuf[0] == escape && escape != 0){
					printf("\n");
					cmdmode();
				} else
#endif  /* MSDOS */
				if(current->parse != NULLFP)
					(*current->parse)(ttybuf,cnt);
			break;
		}
		if(mode == CMD_MODE){
			printf(prompt);
			fflush(stdout);
		}
	}  /* while */
}

main(argc,argv)
int argc;
char *argv[];
{
	static char inbuf[BUFSIZ];      /* keep it off the stack */
	char *fgets();
	int cmdparse();
	void check_time(),ip_recv(),ioinit();
	FILE *fp;
#ifdef  UNIX
	char *getenv();
#endif
#ifdef MSDOS
	int getds();
#ifdef PLUS
	int c;
#else
	void chktasker();
#endif
#endif

#ifdef CPUSTAT
	void cycle();
#endif

#ifdef W5GFE
	/* BKW -- W5GFE  - this to let reverse video option work */
	setupterm((char *) 0,1,(int *) 0);
#endif

#ifdef  SYS5
	if (signal(SIGINT, SIG_IGN) == SIG_IGN) {
		background++;
/*    daemon(); interferes with nohup.out  K5JB  */
	} else
		ioinit();

#else
		ioinit();
#endif

#ifdef PLUS
		c = peek(0xffff,0x0e) & 0xff;
		switch(c) {
			case 0xb6:             /* Portable */
				model=1;break;
			case 'A':
			case 'B':              /* Portable Plus */
				model=0;break;
			default:               /* PC */
				model=2;
		}
/*
 * set: cursor to block, attributes off, keyboard to ALT mode,
 *      transmit functions off, use HP fonts
 */
		printf(/* "\033*dK"      cursor to block                */
			  "\033&d@"        /* display attributes off         */
				 /* "\033&k1\\"    KBD to ALT mode                */
				 /* "\033&s1A"     transmit functions on          */
			  "\033[11m");     /* use ALT fonts                  */
#endif

#if defined(MSDOS) && !defined(PLUS)
		chktasker();
#endif

#ifdef  SYS5
		if (!background) {
#endif
		printf(" Internet Protocol Package, (C) 1988 by Phil Karn, KA9Q\n");

#ifdef NETROM
		printf("      NET/ROM Support (C) 1989 by Dan Frank, W9NK\n\n") ;
#endif
#ifndef AX25
		printf("       Note: This version lacks AX.25 functions!\n\n");
#else
#endif
		printf("             This puppy was housebroken by\n");
		printf("     Joe Buswell, K5JB, 44.78.0.2, Midwest City OK\n\n");
#ifdef  MSDOS
		printf("      MS-DOS version - DS = 0x%x. Heap = %u\n",getds(),
			heapsize);
#endif /* MSDOS */
#ifdef _OSK
		printf(" OS9-68k Port, Robert E. Brose II, N0QBJ, 44.94.249.29, St. Paul, MN\n\n");
#endif

		version();  /* do formatting in version.c */

/* version will include options like Unix, DRSI, MULPORT, etc. */

#ifdef  SYS5
		}
#endif
		fflush(stdout);
		sessions = (struct session *)calloc(nsessions,sizeof(struct session));
		fileinit(argv[1]);	/* added to permit environmental control K5JB */
		fp = fopen(startup,"r");	/* and permit alternative startup files */
		if(fp != NULLFILE){
			while(fgets(inbuf,BUFSIZ,fp) != NULLCHAR){
#ifdef MSDOS
				if(inbuf[0] == 0x1a)	/* Ctrl-Z - prevent error message */
					break;
#endif
				cmdparse(cmds,inbuf);
			}
			fclose(fp);
		}
#ifdef XOBBS
		axinit();
#endif
		cmdmode();

#ifdef CPUSTAT
			cycle(1);	/* initialize cycle counter */
#endif
		/* Main commutator loop */
		for(;;){
#ifdef CPUSTAT
			cycle(0);
#endif
			check_kbd();
			keep_things_going();
		}
}
