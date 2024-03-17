/*
 *	Simple mail user interface for KA9Q IP/TCP package.
 *	A.D. Barksdale Garbee II, aka Bdale, N3EUA
 *	Copyright 1986 Bdale Garbee, All Rights Reserved.
 *	Permission granted for non-commercial copying and use, provided
 *	this notice is retained.
 *	Copyright 1987 1988 Dave Trulli NN2Z, All Rights Reserved.
 *	Permission granted for non-commercial copying and use, provided
 *	this notice is retained.
 *
 * revision history:
 *  3.3.1n Corrected error in line printed to record file before message.
 *  Added -r switch to specify rc file from command line
 *  3.3.1m Altered display of ELM messages will ill formatted date.  Swept
 *  through files doing general cleanup.  Added a config.h
 *  Combined N0QBJ's and my changes and revised version to 3.3.1l - K5JB
 *  changed writefile so it defaults to the folder dir (for osk only) N0QBJ
 *   930130
 *  added OSK defines. N0QBJ 930105
 *  v3.3.1k 920102 Reduced test length of date string from 17 to 15 to catch
 *  lines that don't have seconds included.  Increased wordwrap backup from
 *  9 to 23.
 *  v3.3.1h 911216 Added word wrap to mail sending.  Cut maximum line down
 *  to 79 chars plus possible c/r in sending and reading mail to stop screen
 *  from scrolling from excessive characters on a line.  Fixed raw and cooked
 *  mode for the Unix version so word wrap worked.  Replaced the gets()
 *  functions with fgets() and rip() - K5JB
 *  v3.3.1g 910901 Damn!  I found I was using three environmental var-
 *  iables for spool.  Decided on NETSPOOL, which net was already using.
 *  v3.3.1f 910826 Noticed that handling of time zone was inconsistent
 *  between Unix and MS-DOS, and that it was possible for a user to
 *  over-write an array with an ill formed file, or environmental vbl, 
 *  so fixed both.
 *  v3.3.1e 910813 Added ability of bm.rc to over-ride evironmental
 *  variables in mqueue and maildir, at suggestion of N5OWK
 *  v3.3.1d 900129 Added ability to use environmental variables NETHOME
 *  and MAILSPOOL. In MS-DOS, defaults are as though NETHOME=/net and
 *  MAILSPOOL=/spool.  In Unix, defaults are as though NETHOME=/usr/net
 *  and MAILSPOOL=/usr/spool.  These are consistent with version of net
 *  that I run.  Prevented possible buffer over-run in send.c.  ifdefed
 *  with PRINTER the print functions. - K5JB
 *  v3.3.1c 890902 Turbo C compiled version.  __TURBOC__ is defined in
 *  the compiler - Made several fixes to end of line handling (again), and
 *  added the zone XXX recognition in bm.rc for the MS-DOS version. - K5JB
 *  v3.3.1b attempt to format input in send.c by clumsy operator over-running
 *  buffer.  Abandoned - Unix buffered I/O takes care of it. (see rev d)
 *  v3.3.1a 890722 Corrected message numbering, allowed for $NETHOME to
 *  hold .bmrc. (see rev d) J. Buswell, K5JB
 *  v3.3 870467 D. Trulli nn2z
 *  v3.2 870314 D. Trulli nn2z
 *  v3.1 870117 D. Trulli nn2z
 *	Add multiple arguments mail commands.
 *	Send to multiple users.
 *	Add status commands;
 *  v3.0 871225 D. Trulli nn2z
 *	Heavily restructured program to use a index array of message.
 *	More compatible with UNIX type mail commands.
 *  v2.7 871214 D. Trulli nn2z
 *	Cleaned up header and message parsing to prevent lock up.
 *	Revised command structure.
 *  v2.6 870826 Bdale
 *	integrate PA0GRI's interface/gateway to the WA7MBL PBBS
 *  v2.5 870713 Bdale
 *      integrate additional patches from PA0GRI, minor cleanup
 *  v2.4 870614 - P. Karn
 *      "smart gateway" function moved to smtp client code in net.exe.
 *	User interface for sending mail reworked to resemble Berkeley Mail
 *  v2.3 870524
 *      Extensive addition/revision by Gerard PA0GRI.  Now supports a healthy
 *      set of real commands.
 *  v2.1,2.2
 *      exact change history lost.
 *  v2.0 c.870115
 *      First version with command parser, only send and quit work.
 *  v1.0
 *      First attempt.  Send messages only.
 */

#include <stdio.h>
#include <ctype.h>

#ifdef	_OSK
#include <strings.h>
#include <dir.h>
#define strchr index
#ifndef	_UCC
char *strpbrk();
#endif
#else
#include <string.h>
#endif	/* _OSK */

#ifdef MSDOS
#include <io.h>
#include <stdlib.h>
#endif
#include "bm.h"
#include "config.h"

extern char version[];
static char copyright1[] =
"Copyright 1987 Garbee N3EUA";
static char copyright2[] =
"Copyright 1988 Trulli NN2Z";
char *margin = "          ";

char *helpmsgs[] = {
	"b [msg]        bounce message (remail)",
	"d [msg]        delete message(s)",
	"f [msg]        forward message",
	"h              display message headers",
	"k [job id]     kill unsent messages",
	"l              list unsent messages",
	"m userlist     mail a message",
	"n [file]       display or change notesfile",
#ifdef SWITCHUSER
	"N              switch to same name as notesfile",
#endif
#ifdef PRINTER
	"p [msg]        print message(s) on printer",
#endif
	"q              quit (see also x)",
	"r [msg]        reply to a message",
	"s [msg] [file] save message(s) in file (default mbox)",
	"u [msg]        undelete message(s)",
	"w [msg] file   save message(s) in file no header",
	"x              quit without changing mail file",
	".              read current message",
	"#              read message number #",
#ifdef	_OSK
	"! cmd          OS9 shell command",
#else
	"! cmd          run dos/unix command",
#endif
	"$              sync the notefile",
	"?              print this help screen",
	""	/* mark the end */
	};

/* commands valid in bm.rc */

struct token rccmds[] = {
	"smtp", SMTP,
	"host", HOST,
	"user", USER,
	"edit", EDIT,
	"fullname", NAME,
	"reply", REPLY,
	"maxlet", MAXLET,
	"mbox", MBOX,
	"record", RECORD,
	"screen", SCREEN,	/* leaving it to avoid error messages */
	"zone",TIMEZONE,	/* Added to Unix version K5JB */
	"folder", FOLDER,
	"mqueue", MQUEUE,
#ifdef UNIX
	"wordwrap",WWRAP,
#endif
	NULLCHAR, 0
};

FILE	*mfile = NULLFILE;
char 	*hostname = NULLCHAR;		/* name of this host from rc file */
char	*username = NULLCHAR;		/* name of this user from rc file */
char	*fullname = NULLCHAR;		/* fullname of this user from rc file */
char	*replyto = NULLCHAR;		/* address for reply-to header */
char	*maildir = NULLCHAR;		/* defined mail directory */
char	*maildir_o = NULLCHAR;	/* Original mail directory */
char	*mqueue = NULLCHAR;		/* defined mqueue outbound directory */
char	*savebox = NULLCHAR;		/* name of the mbox text file */
char	*record = NULLCHAR;		/* record  outbound mail in this file */
char	*folder = NULLCHAR;		/* directory for saveing read mail */
char	*editor = NULLCHAR;		/* user's favorite text editor */
char	notename[9];			/* name of current notesfile */
char	*notename_op;	/* pointer to the original notename */
char	notefile[SLINELEN];		/* full pathname of mail text file */
char	*mfilename = notefile;	/* pointer to current mbox or mail file -f */
int	current;			/* the current message number */
int	nmsgs;			/* the number of messages in the notesfile */
int	newmsgs;		/* Number of new unread message */
int	change;			/* indicates that the mail file has been
				 * changed in this session */
int	fflag = 0;		/* true if current notesfile is not an mbox */
int	qflag = 0;		/* true if bm is used just to queue files */
unsigned maxlet = NLET+1;	/* max number of messages in mailbox */
int	tty = 0;		/* tells if stdin is a tty */
struct let *mbox;		/* pointer to the array of messages */
#ifdef UNIX
int wflag = FALSE;
#endif

#ifdef OPTALIAS
char usage[] = "Usage: (read) bm [-u user] [-f file]\n    or (send) bm [-a aliasfile] [-r rcfile] [-s subject] [-q] addr .. addr\n";
#else
char usage[] = "Usage: (read) bm [-u user] [-f file]\n    or (send) bm [-r rcfile] [-s subject] [-q] addr .. addr\n";
#endif
char badmsg[] = "Invalid Message number %d\n";
char nomail[] = "No messages\n";
char noaccess[] = "Unable to access %s\n";
char *runcom;	/* .rc file K5JB 3.3.1n */
#ifdef OPTALIAS
char *alias;	/* 3.3.1n */
#endif

int
check_maildir(cp)	/* added n3 */
char *cp;
{
#ifdef _OSK
	if((md=opendir(cp))!=NULL)
		closedir(md);
	else
#else
	if(access(cp,0))
#endif
	{
		printf(noaccess,cp);
		return -1;
	}
	return 0;
}

main(argc,argv)
int argc;
char *argv[];
{
	extern int optind;
	extern char *optarg;
	char *subjectline = NULLCHAR;
	long	tmp;
	int	c;
	int	ret;
#ifdef	_OSK
	DIR *md;
#endif

#if	defined(__TURBOC__) || defined(MICROSOFT)
	(void) fclose(stdaux);
	(void) fclose(stdprn);
#endif

	while ((c = getopt(argc,argv,"u:f:s:r:a:q?")) != -1) {
		switch(c) {
		case 'f':
			fflag++;
			mfilename = optarg;
			break;
		case 'q':
			qflag++;
			break;
		case 's':
			subjectline = optarg;
			break;
		case 'u':
			strncpy(notename,optarg,8);
			notename_op = savestr(notename);
			break;
		case 'r':	/* K5JB 3.3.1n */
			runcom = optarg;
			break;
#ifdef OPTALIAS
		case 'a':	/* K5JB 3.3.1n */
			alias = optarg;
			break;
#endif
		case '?':
			printf(usage);
			exit(1);
		}
	}
	loadconfig();

	if(!*notename){
		strncpy(notename,username,8);
		notename_op = savestr(notename);
	}

	if (qflag == 0 && isatty(fileno(stdin))) {
		/* announce ourselves */
#if	!defined(UNIX) && defined(CLEAR)
		screen_clear();
#endif
#ifdef C386
		printf("      %s%s 80386 (K5JB)\n%s%s -- %s\n\n",margin,version,margin,
				copyright1,copyright2);
#else
		printf("%s%s%s (K5JB)\n%s %s -- %s\n\n",margin,margin,version,margin,
				copyright1,copyright2);
#endif
		tty = 1;
	}

	current = 1;
	nmsgs = 0;

	/* check for important directories */
	if(check_maildir(maildir))
		exit(1);
#ifdef	_OSK
	if((md=opendir(mqueue))!=NULL)
		closedir(md);
	else
#else
	if(access(mqueue,0))
#endif
	{
		printf(noaccess,mqueue);
		exit(1);
	}

#ifdef SIGNALS
	/* set any signal handlers to catch break */
	setsignals();
#endif

	if(optind < argc){
		dosmtpsend(NULLFILE,&argv[optind],argc-optind,subjectline);
		(void) exit(0);
	}

	tmp = (long)maxlet * (long)sizeof(struct let);
#ifdef	MSDOS
	/*
	* Since we are in the dos small model make sure that we
	* don't overflow a unsigned short on the number bytes
	* need for malloc. If not checked malloc will
	* succeed and we will be trashing ourself in no time.
	*/
	if ((tmp & 0xffff0000l) ||
		 (mbox=(struct let *)malloc((unsigned short)tmp)) == (struct let *)0)
#else
	if ((mbox=(struct let *)malloc((unsigned)tmp)) == (struct let *)0)
#endif
	{
		fprintf(stderr,
		"Can't allocate memory table for %d messages\n",
		    maxlet);
		(void) exit(1);
	}

	/* there is slight risk of array over-write here, and elsewhere when
	sprintf is used.  I'm may cut LINELEN to 88 (malloc does things by
	8 and 81 would have been enough for the wordwrapping - K5JB */
	sprintf(notefile,"%s/%s.txt",maildir,notename);
	if (!fflag && lockit())
		exit(1);
	ret = initnotes();
#if defined(MSDOS) && defined(DVREPORT)
	printf("%s%sDV and Windows users: coreleft is %u\n\n",
		margin,margin,coreleft());
#endif
	if (!fflag)
		rmlock(maildir,notename);
	if (ret != 0)
		exit(1);
	listnotes();

	getcommand();
	return 0;
}

void
loadconfig()
{
	FILE	*rcfp;	/* handle for the configuration file */
	char	rcline[LINELEN]; /* buffer for config file reading */
	register char *s,*p;
	extern char *mailspool;
	extern char *mailqdir;
	extern char *spool;
#ifdef OPTALIAS
	extern char *default_alias;
#endif
	extern char *alias;	/* could be defined in this file, or in files.c */
	extern char *default_runcom;
	int line = 0;
	char *getenv();
   extern char timez[];	/* made available to Unix too - K5JB */

	/* if either of these is set at command line, we use it - K5JB */
	if(!*runcom)
		runcom = default_runcom;
#ifdef OPTALIAS
	if(!*alias)
		alias = default_alias;
#endif

#ifdef UNIX
	/* first try $NETHOME/runcom.  We give it two shots.  K5JB */
	if ((p = getenv(HOMEDIR)) != NULLCHAR || (p = getenv("HOME")) != NULLCHAR)
#else
	if ((p = getenv(HOMEDIR)) != NULLCHAR)
#endif
		sprintf(rcline, "%s/%s", p, runcom);
	else
#ifdef UNIX
		/* if neither is defined we use current dir for Unix */
		sprintf(rcline, "./%s", runcom);
#else
		/* only in MS-DOS would we use root */
		sprintf(rcline, "/%s", runcom);
#endif
	runcom = savestr(rcline);

	/* and also locate our alias file */
	if(p == NULLCHAR)
#ifdef UNIX
		sprintf(rcline,"./%s",alias);
#else
		sprintf(rcline,"/%s",alias);
#endif
	else
		sprintf(rcline,"%s/%s",p,alias);

	alias = savestr(rcline);

	if ((rcfp = fopen(runcom,"r")) == NULLFILE) {	/* open config file */
		printf("Cannot open '%s', check your installation\n",runcom);
		(void) exit(1);
	}
	while (!feof(rcfp)) {
		if (fgets(rcline,LINELEN,rcfp) == NULLCHAR)
			break;
		line++;
		rip(rcline);
		if (*rcline == '\0' || *rcline == ';'|| *rcline == '#')
			continue;
		/* find the argument to the command */

		s = rcline;
		/* skip the white space */
		while(*s == ' ' || *s == '\t')
				s++;
		p = s;
		/* skip the command */
		while(*p && *p != ' ' && *p != '\t')
				p++;
		/* skip the white space */
		while(*p == ' ' || *p == '\t')
				p++;
		if (*s == '\0')
			continue;

		switch (rc_line_type(s)) {
		case HOST:
			hostname = savestr(p);
			break;
		case USER:
			username = savestr(p);
			break;
		case REPLY:
			replyto = savestr(p);
			break;
		case EDIT:
			editor = savestr(p);
			break;
		case SMTP:
			maildir_o = maildir = savestr(p);
			break;
		case NAME:
			fullname = savestr(p);
			break;
		case MAXLET:
			maxlet = atoi(p)+1;
			break;
		case MBOX:
			savebox = savestr(p);
			break;
		case RECORD:
			record = savestr(p);
			break;
      case TIMEZONE:
         strcpy(timez,p);
         break;
		case SCREEN:
#ifdef VIDEO
			setvideo(p);
#endif
			break;
		case FOLDER:
			folder = savestr(p);
			break;
		case MQUEUE:
			mqueue = savestr(p);
			break;
#ifdef UNIX
		case WWRAP:
			wflag = TRUE;
			break;
#endif
		default:
			fprintf(stderr,
			"%s: line %d Invalid command: '%s'\n",
			runcom,line,rcline);
			exit(1);
		}
	}
	(void) fclose(rcfp);
	if ((p = getenv("NETSPOOL")) == NULLCHAR)	/* K5JB NET compatibility */
		p = spool;
	if(maildir == NULLCHAR){
		sprintf(rcline, "%s/%s", p,mailspool);
		maildir_o = maildir = savestr(rcline);
	}
	if(mqueue == NULLCHAR){
		sprintf(rcline,"%s/%s",p,mailqdir);
		mqueue = savestr(rcline);
	}
	if(savebox == NULLCHAR){
		sprintf(rcline,"%s/%s",maildir,"mbox");
		savebox = savestr(rcline);
	}
	if(hostname == NULLCHAR) {
		fprintf(stderr,"%s: hostname not set\n",runcom);
		exit(1);
	}
	if(username == NULLCHAR) {
		fprintf(stderr,"%s: username not set\n",runcom);
		exit(1);
	}
}

/* return the line_type from a line of the configuration file */
rc_line_type(s)
register char *s;
{
	register struct token *tp;
	for (tp = rccmds; tp->str != NULLCHAR; tp++) {
		if (strncmp(tp->str,s,strlen(tp->str)) == 0)
			return tp->type;
	}
	return (NONE);
}

/* replace terminating end of line marker(s) with null */
void
rip(s)
register char *s;
{
	for (; *s; s++)
		if (*s == '\r' || *s == '\n') {
			*s = '\0';
			break;
		}
}

/* copy a string return a pointer to it */
char *
savestr(s)
char *s;
{
	register  char *p;
	p = (char *)malloc(strlen(s)+1);
	if (p == NULLCHAR)
		printf("No more memory\n");
	else
		strcpy(p,s);
	return p;
}


void
dohelp()
{
	int i;

#if	!defined(UNIX) && defined(CLEAR)
	screen_clear();   /* I REALLY DON'T like screen clearing - K5JB */
#endif
	for(i=0;*helpmsgs[i];i++)
		printf("%s%s\n",margin,helpmsgs[i]);
}


/* save a message list in a file in mailbox format */
void
savemsg(argc,argv)
int argc;
register char *argv[];
{
	register char *savefile;
	int msgnum;
	int	i;
	FILE *tfile;
	char buf[LINELEN];

	if (mfile == NULLFILE){
		printf(nomail);
		return;
	}
	if (argc == 0 || isdigit(*argv[argc - 1])) {
		savefile = savebox;
	} else {
		savefile = argv[argc - 1];
		--argc;
		/* if it is just a file name and not a path name then check
		** for a folder path defined and use that path to
		** save the file.
		*/
		if (strpbrk(savefile,"/\\") == NULLCHAR && folder != NULLCHAR) {
			sprintf(buf,"%s/%s",folder,savefile);
			savefile = buf;
		}
	}
	if ((tfile = fopen(savefile,"a")) == NULLFILE) {
		perror(savefile);
		return;
	}
	if (argc == 0)
		msgtofile(current, tfile, 0);
	else {
		for (i = 0; i < argc; i++) {
			msgnum = atoi(argv[i]);
			if (msgnum >= 1 && msgnum <= nmsgs)
				msgtofile(msgnum, tfile, 0);
			else
				printf(badmsg,msgnum);
		}
	}
		(void) fclose(tfile);
		printf("%s appended\n",savefile);
}

#ifdef PRINTER
/* send messages to the print device */
printmsg(argc,argv)
int argc;
char *argv[];
{
	FILE *prn;
	int msgnum;
	int	i;

	if (mfile == NULLFILE){
		printf(nomail);
		return;
	}
	if ((prn = fopen("PRN","a")) == NULLFILE) {
		perror("PRN");
		return;
	}
	if (argc == 0)
		msgtofile(current, prn, 0);
	else {
		for (i = 0; i < argc; i++) {
			msgnum = atoi(argv[i]);
			if (msgnum >= 1 && msgnum <= nmsgs)
				msgtofile(msgnum, prn, 0);
			else
				printf(badmsg,msgnum);
		}
	}
	fclose(prn);
}
#endif

void
writemsg(argc,argv)
int argc;
char *argv[];
{
	char *writefile;
	int msgnum;
	int	i;
	FILE *tfile;
#ifdef	_OSK
	char buf[LINELEN];
#endif

	if (mfile == NULLFILE){
		printf(nomail);
		return;
	}
	if (argc == 0 || isdigit(*argv[argc - 1])) {
		printf("no file specified\n");
		return;
	} else {
		writefile = argv[argc - 1];
		--argc;
	}
#ifdef _OSK
	if (strpbrk(writefile,"/\\") == NULLCHAR && folder != NULLCHAR) {
		sprintf(buf,"%s/%s",folder,writefile);
		writefile = buf;
	}
#endif
	if ((tfile = fopen(writefile,"a")) == NULLFILE) {
		perror(writefile);
		return;
	}
	if (argc == 0)
		msgtofile(current, tfile, 1);
	else {
		for (i = 0; i < argc; i++) {
			msgnum = atoi(argv[i]);
			if (msgnum >= 1 && msgnum <= nmsgs)
				msgtofile(msgnum, tfile, 1);
			else
				printf(badmsg,msgnum);
		}
	}
	(void) fclose(tfile);
	printf("%s appended\n",writefile);
}

void
bmexit(x)
int x;
{
	if(!fflag && lockit())
		exit(1);
	(void) closenotes();
	if (!fflag)
		rmlock(maildir,notename);
	exit(x);
}

/* correct most common keyboard mistake - 3.3.1n1 */
int
checkformat(nargs,args)
int nargs;
char *args[];
{
	int i;
	for(i=0;i<nargs;i++)
		if(args[i][0] == '@' || args[i][strlen(args[i]) - 1] == '@'){
			printf("Don't put spaces in user@host\n");
			return -1;
		}
	return 0;
}

/* this is the main command processing loop */
void
getcommand()
{
	FILE *tfile, *tmpfile();
	char	command[LINELEN];	/* command line */
	char	*args[MAXARGS];
	int	nargs;
	char	*cp,*mcp;	/* mcp is moved command pointer v 1n2 */
	register int	msgnum;
	register int	i;

	printf("\nType ? for help.\n");

	/* command parsing loop */
	while(1) {
		printf("\"%s\"> ",notename);
		fgets(command,LINELEN,stdin);
		rip(command);
		if(*command == '!') {
			if (system(&command[1]))
				perror("system");
			continue;
		}

		/* Fix leading spaces entered accidentally - version 1n2 */
		cp = command;
		while(*cp == ' ')
			cp++;

		mcp = cp;

		if (*cp) {
			while (*cp && *cp != ' ')
				cp++;
			nargs = parse(cp,args,MAXARGS);
		}

		switch (*mcp) {
		case 'm':		/* send msg */
			if (nargs == 0) {
				printf("To: ");
				fgets(command,LINELEN,stdin);
				rip(command);
				nargs = parse(command,args,MAXARGS);
			}
			if(!checkformat(nargs,args))
				dosmtpsend(NULLFILE,args,nargs,NULLCHAR);
			break;

		case 's':		/* save current msg to file */
			savemsg(nargs,args);
			break;

		case 'w':		/* write current msg to file */
			writemsg(nargs,args);
			break;

		case 'x':		/* abort */
			(void) fclose(mfile);
			(void) exit(0);
			/* NOTREACHED */
			break;

#ifdef PRINTER
		case 'p':		/* print message */
			printmsg(nargs,args);
			break;
#endif

		case 'r':			/* reply */
			if (nargs == 0)
				msgnum = current;
			else
				msgnum = atoi(args[0]);
			if (msgnum >= 1 && msgnum <= nmsgs)
				reply(msgnum);
			else
				printf(badmsg,msgnum);
			break;

		case 'f':
			if(nargs == 0)
				msgnum = current;
			else
				msgnum = atoi(args[0]);
			if (msgnum < 1 || msgnum > nmsgs) {
				printf(badmsg,msgnum);
				break;
			}
			if((tfile = tmpfile()) == NULLFILE)
				printf("\nCannot open temp file\n");
			else {
				msgtofile(msgnum,tfile,0);
				fseek(tfile,0L,0);
				printf("To: ");
				fgets(command,LINELEN,stdin);
				rip(command);
				nargs = parse(command,args,MAXARGS);
				if(!checkformat(nargs,args))
					dosmtpsend(tfile,args,nargs,NULLCHAR);
#if defined(_OSK) && !defined(_UCC)
				tmpclose(tfile);
#else
				(void) fclose(tfile);
#endif
			}
			break;

		case 'b':		/* bounce a message */
			if(nargs == 0)
				msgnum = current;
			else
				msgnum = atoi(args[0]);
			if (msgnum < 1 || msgnum > nmsgs) {
				printf(badmsg,msgnum);
				break;
			}
			if((tfile = tmpfile()) == NULLFILE)
				printf("\nCannot open temp file\n");
			else {
				msgtofile(msgnum,tfile,0);
				fseek(tfile,0L,0);
				printf("To: ");
				fgets(command,LINELEN,stdin);
				rip(command);
				nargs = parse(command,args,MAXARGS);
				if(!checkformat(nargs,args))
					bouncemsg(tfile,args,nargs);
#if defined(_OSK) && !defined(_UCC)
				tmpclose(tfile);
#else
				(void) fclose(tfile);
#endif
			}
			break;

		case 'u':
			if (nargs == 0)
				mbox[current].status &= ~DELETE;
			else
				for (i = 0; i < nargs; i++) {
					msgnum = atoi(args[i]);
					if( msgnum >= 1 && msgnum <= nmsgs)
						mbox[msgnum].status &= ~DELETE;
					else
						printf(badmsg,msgnum);
				}
			break;

		case 'l':		/* display unsent messages */
			listqueue();
			break;
		case 'k':
			if (nargs == 0)
				printf("No job id specified\n");
			else
				for (i = 0; i < nargs; i++)
					killjob(args[i]);
			break;

		case 'n': 	/* display or change notefiles */
			mboxnames(nargs,args);
			break;
#ifdef SWITCHUSER
		case 'N':	/* switch to same name as notename */
			username = notename;
			break;
#endif
		case 'q':		/* quit */
			if (isnewmail()) {
				printf("New mail has arrived\n");
				reinit();
			} else
				bmexit(0);
			/* NOTREACHED */
			break;
		case '$':
			reinit();
			break;

		case 'd':		/* delete a message */
			if (nargs == 0)
				delmsg(current);
			else
				for ( i = 0; i < nargs; i++) {
					msgnum = atoi(args[i]);
					if( msgnum >= 1 && msgnum <= nmsgs)
						delmsg(msgnum);
					else
						printf(badmsg,msgnum);
				}
			break;

		case 'h':		/* list message headers in notesfile */
			listnotes();
			break;

		case '\0':	/* a blank line prints next message */
			printnext();
			break;

		case '?':		/* help */
			dohelp();
			break;

		case  '.':
			displaymsg(current);
			break;
		default:
			if (!isdigit(*mcp))
				printf("Invalid command - ? for help\n");
			else {
				msgnum = atoi(mcp);
				if (msgnum < 0 || msgnum > nmsgs)
					printf(badmsg,msgnum);
				else {
					current = msgnum;
					displaymsg(current);
				}
			}
			break;
		}
	}
}

/* list or change mbox */
void
mboxnames(argc,argv)
int argc;
char *argv[];
{
	register char *cp;
	int ret;
	char    line[16];       /* filedir will only return 15 max */
	char	buf[LINELEN];

	if(argc != 0) {
		if(!fflag && lockit())
			return;
		ret = closenotes();
		if (!fflag)
			rmlock(maildir,notename);
		if (ret != 0)
			exit(1);
		/* argv is going away so we gotta do this differently, and incidentally
		 * complete the job (ver.n3) - K5JB
		 */
		if (strpbrk(argv[0],"/\\.") != NULLCHAR) {
			char *cp,*cp2,*larg;
			if((larg = (char *)malloc(strlen(argv[0]) + 1)) == NULLCHAR)
				exit(1);
			sprintf(larg,"%s",argv[0]);
			for(cp = larg;*cp;cp++)	/* normalize the argument, permit dot also */
				if(*cp == '\\' || *cp == '/' || *cp == '.')
					*cp = '/';	/* MSDOS doesn't care but Unix does */
			if(*larg == '/'){	/* leading / restores startup settings */
				if(maildir != maildir_o){
					free(maildir);
					maildir = maildir_o;	/* restore original maildir */
				}
				sprintf(notename,"%s",notename_op);	/* restore original notename */
				sprintf(notefile,"%s/%s.txt",maildir,notename);
			}else{
				if((cp = (char *)malloc(strlen(maildir) + strlen(larg) + 2)) == NULLCHAR)
					exit(1);
				/* assume argv[0] is "k5jb/cray" format. maildir had no trailing / */
				sprintf(cp,"%s/%s",maildir,larg);
				cp2 = strrchr(cp,'/');
				*cp2 = '\0';	/* remove the "/cray" */
				if(check_maildir(cp)){
					free(cp);
					free(larg);
					return;
				}	/* we will use cp in a bit for maildir */
				if(larg[strlen(larg) - 1] == '/')	/* "k5jb/" used */
					sprintf(notefile,"%s/%s%s.txt",maildir,larg,notename);
				else
					sprintf(notefile,"%s/%s.txt",maildir,larg);
				if(maildir != maildir_o)
					free(maildir);
				maildir = cp;
				cp = strrchr(larg,'/');
				if(*++cp)	/* n k5jb/ will get us to the subdirectory, no notename change */
					strncpy(notename,cp,8);
			}
			free(larg);
		} else {	/* no /, \, or dot */
			fflag = 0;
			strncpy(notename,argv[0],8);
			sprintf(notefile,"%s/%s.txt",maildir,notename);
		}
		mfilename = notefile;
		if (!fflag && lockit()) {
			mfile = NULLFILE;
			printf("Mail file is busy\n");
			return;
		}
		ret = initnotes();
		if (!fflag)
			rmlock(maildir,notename);
		if (ret != 0)
			exit(1);
		listnotes();

	} else {  /* he wants to see what notefiles there are */
		sprintf(buf,"%s/*.txt",maildir,notename);
		filedir(buf,0,line);
		while(line[0] != '\0') {
			cp = strchr(line,'.');
			*cp = '\0';
			printf("notesfile -> %s\n",line);
			filedir(buf,1,line);
		}
	}
}

void
reinit()
{
	int ret;
	if (!fflag && lockit())
		return;
	ret = closenotes();
	if (!fflag)
		rmlock(maildir,notename);
	if (ret != 0)
		exit(1);
	if (!fflag && lockit()) {
		mfile = NULLFILE;
		printf("Mail file is busy\n");
		return;
	}
	ret = initnotes();
	if (!fflag)
		rmlock(maildir,notename);
	if (ret != 0)
		exit(1);
	listnotes();
}
