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
#endif
#include "bm.h"

#undef PRINTER

extern char version[];
static char copyright1[] =
"Copyright 1987 Garbee N3EUA";
static char copyright2[] =
"Copyright 1988 Trulli NN2Z";

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
	"screen", SCREEN,
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
char	*mqueue = NULLCHAR;		/* defined mqueue outbound directory */
char	*savebox = NULLCHAR;		/* name of the mbox text file */
char	*record = NULLCHAR;		/* record  outbound mail in this file */
char	*folder = NULLCHAR;		/* directory for saveing read mail */
char	*editor = NULLCHAR;		/* user's favorite text editor */
char	notename[9];			/* name of current notesfile */
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
int wflag = FALSE;

char usage[] = "Usage: bm [-u user] [-f file] \n  or bm [-s subject] [-q] addr .. addr\n";
char badmsg[] = "Invalid Message number %d\n";
char nomail[] = "No messages\n";
char noaccess[] = "Unable to access %s\n";

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

	loadconfig();
	if (qflag == 0 && isatty(fileno(stdin))) {
		/* announce ourselves */
#if	!defined(UNIX) && defined(CLEAR)
		screen_clear();
#endif
		printf(" %s\n%s -- %s\n\n",version,copyright1,copyright2);
		tty = 1;
	}

	current = 1;
	nmsgs = 0;

	/* check for important directories */
#ifdef _OSK
	if((md=opendir(maildir))!=NULL)
		closedir(md);
	else
#else
	if(access(maildir,0))
#endif
	{
		printf(noaccess,maildir);
		exit(1);
	}
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
	strncpy(notename,username,8);
	notename[8] = '\0';

	while ((c = getopt(argc,argv,"u:f:s:q")) != -1) {
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
			notename[8] = '\0';
			break;
		case '?':
			printf(usage);
			exit(1);
		}
	}

	/* set any signal handlers to catch break */
	setsignals();

	if(optind < argc){
		dosmtpsend(NULLFILE,&argv[optind],argc-optind,subjectline);
		(void) exit(0);
	}

	tmp = (long)maxlet * (long)sizeof(struct let);
#ifdef	MSDOS
	/*
	* Since we are in the dos small model make sure that we
	* don't overflow a unsigned short on the number bytes
	* need for mallloc. If not checked malloc will
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
	/* This is useful for desqview tuning */
#ifdef MSDOS
	printf("Coreleft: %u\n",coreleft());
#endif

	/* there is slight risk of array over-write here, and elsewhere when
	sprintf is used.  I'm may cut LINELEN to 88 (malloc does things by
	8 and 81 would have been enough for the wordwrapping - K5JB */
	sprintf(notefile,"%s/%s.txt",maildir,notename);
	if (!fflag && lockit())
		exit(1);
	ret = initnotes();
#ifdef MSDOS
	printf("After initnotes, coreleft is %u\n",coreleft());
#endif
	if (!fflag)
		rmlock(maildir,notename);
	if (ret != 0)
		exit(1);
	listnotes();

	getcommand();
	return 0;
}

loadconfig()
{
	FILE	*rcfp;	/* handle for the configuration file */
	char	rcline[LINELEN]; /* buffer for config file reading */
	register char *s,*p;
/*
	char q[40];
	char *q;
*/
	extern char *alias;
	extern char *mailspool;
	extern char *mailqdir;
	extern char *spool;
	int line = 0;
	char *runcom;
	char *getenv();
   extern char timez[];	/* made available to Unix too - K5JB */

	/* first try $NETHOME/RUNCOM 	K5JB */
	if ((p = getenv("NETHOME")) == NULLCHAR)
#ifdef UNIX
		sprintf(rcline, "./%s", RUNCOM);
#else
		sprintf(rcline, "/%s", RUNCOM);
#endif
		else
			sprintf(rcline, "%s/%s", p, RUNCOM);
	runcom = savestr(rcline);
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
			maildir = savestr(p);
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
#if	defined(__TURBOC__)
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
	if ((p = getenv("NETSPOOL")) == NULLCHAR)	/* Blush!  I hate this! */
		p = spool;
	if(maildir == NULLCHAR){
		sprintf(rcline, "%s/%s", p,mailspool);
		maildir = savestr(rcline);
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

dohelp()
{
#if	!defined(UNIX) && defined(CLEAR)
	screen_clear();   /* I REALLY DON'T like screen clearing - K5JB */
#endif
	printf("\n\n");
	printf(" d [msglist]           delete a message\n");
	printf(" m userlist            mail a message\n");
	printf(" s [msglist] [file]    save message in file (default mbox)\n");
	printf(" w [msglist] file      save message in file no header\n");
	printf(" f [msg]               forward message\n");
	printf(" b [msg]               bounce message (remail)\n");
	printf(" r [msg]               reply to a message\n");
	printf(" u [msglst]            undelete a message\n");
	printf(" p [msglst]            print message on printer (DOS only)\n");
	printf(" .                     display current message\n");
	printf(" h                     display message headers in notefile\n");
	printf(" l                     list unsent messages\n");
	printf(" k                     kill unsent messages\n");
	printf(" n [file]              display or change notesfile\n");
	printf(" #                     where # is the number of message to read\n");
	printf(" x                     quit without changing mail file\n");
	printf(" q                     quit\n");
#ifdef	_OSK
	printf(" ! cmd				   OS9 shell command\n");
#else
	printf(" ! cmd                 run dos/unix command\n");
#endif
	printf(" $                     sync the notefile\n");
	printf(" ?                     print this help screen\n");
}


/* save a message list in a file in mailbox format */
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

/* this is the main command processing loop */
getcommand()
{
	FILE *tfile, *tmpfile();
	char	command[LINELEN];	/* command line */
	char	*args[MAXARGS];
	int	nargs;
	char	*cp;
	register int	msgnum;
	register int	i;

	printf("\nType ? for help.\n");

	/* command parsing loop */
	while(1) {
		printf("\"%s\"> ",notename);
		fgets(command,LINELEN,stdin);
#ifdef OLD	/* this has undesirable side effects */
		if (feof(stdin))		/* someone hit ^Z! */
			strcpy(command,"q");	/* treat it like 'q' */
#endif
		rip(command);
		if(*command == '!') {
			if (system(&command[1]))
				perror("system");
			continue;
		}
		if (*command) {
			cp = command;
			while (*cp && *cp != ' ')
				cp++;
			nargs = parse(cp,args,MAXARGS);
		}

		switch (*command) {
		case 'm':		/* send msg */
			if (nargs == 0) {
				printf("To: ");
				fgets(command,LINELEN,stdin);
				rip(command);
				nargs = parse(command,args,MAXARGS);
			}
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

		case 'p':		/* print message */
#ifdef PRINTER
			printmsg(nargs,args);
#else
			printf("Command not available in this version\n");
#endif
			break;

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
			if (!isdigit(*command))
				printf("Invalid command - ? for help\n");
			else {
				msgnum = atoi(command);
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
			if (strpbrk(argv[0],"/\\") != NULLCHAR) {
				fflag = 1;
				mfilename = argv[0];
			} else {
				fflag = 0;
				mfilename = notefile;
				strncpy(notename,argv[0],8);
				notename[8] = '\0';
				sprintf(notefile,"%s/%s.txt",maildir,notename);
			}
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
