/* fixed possible buffer overflow by operator with broke C/R key - K5JB */
/* added time zone things to make Unix and MS-DOS code the same */
/* 12/13/91 Added wordwrapping as an option for mail entering - K5JB */

#include <stdio.h>
#ifdef	_OSK
#include <strings.h>
#define strchr index
#define strrchr rindex
#else
#include <string.h>
#endif
#include <time.h>
#include <ctype.h>

#ifdef MSDOS
#include <conio.h>
#include <dir.h>
#include <stdlib.h>
#endif
#if	((defined(UNIX) || defined(MICROSOFT)) && !defined(_OSK))
#include <sys/types.h>
#include <termio.h>
#include <fcntl.h>
#endif

#include "bm.h"

struct addr *addrecip();
char *ptime();

#define WRAP_AT 79	/* must be one shorter than a terminal line */
#define MAXWRAP 23	/* maximum characters it will back up looking for space */
#ifdef UNIX
extern int wflag;	/* wordwrap flag for Unix */
int getrch();
int setrawmode();
int setcookedmode();
#endif	/* UNIX */

/* a word wrapping line getter, returns without an EOL - K5JB */

int
lgets(tmpline,wordwrap,maxline)
char *tmpline; /* this will contain a good line */
char *wordwrap;	/* this contains what we backed up to wrap to next line */
int maxline;
{
	int i = 0,j;
	char *tlp = tmpline;
	char *wrp = wordwrap;

	if(*wrp != '\0'){
		while(*wrp++ != '\0')	/* find end of string */
		;
		wrp -= 2;		/* back up to last character */
		do{
			*tlp++ = *wrp--;   /* copy in reverse order to tmpline */
			i++;
			}while(wrp >= wordwrap);
		*tlp = '\0';
		printf("\n%s",tmpline);
	}
	wrp = wordwrap;
	*wrp = '\0';
	for(;i<maxline;i++){
#ifdef UNIX
#ifdef COH386
	while((*tlp = (char)getrch()) == -1)
#else
	while((*tlp = (char)getrch()) == 0xff)
#endif
		;	/* in the Unix and Coherent, idle keyboard returns -1 and
	there appears to be a sign extension problem here */
		switch(*tlp)
#else
		switch(*tlp = (char)getch())
#endif
		{
			case '\b':
				if(i){
					i -= 2;
					tlp--;
					printf("\b \b");
				}
				break;
			case '\r':
			case '\012':	/* try to deal with J's wierd setup */
					printf("\n");
					*tlp = '\0';
					wordwrap[0] = '\0';
					return(i);
			default:
				printf("%c",*tlp);
				tlp++;
		}	/* switch */
	} /* i == maxline */
	*tlp = '\0';
	tlp -= 1;	/* back up to last character typed */
	i--;
	for(j=0;j<MAXWRAP && tlp >= tmpline;j++){
		printf("\b \b");
		if(*tlp == ' '){
			*tlp = '\0';
			*wrp = '\0';
			if(!j)
				printf("\n");
			return(i-j);
		}
		*wrp++ = *tlp--;
	}
	/* if we get here, we failed to find a space */
	tlp++;
	for(j=0;j < MAXWRAP;j++)
		printf("%c",*tlp++);
	printf("\n");
	*tlp = '\0';
	wordwrap[0] = '\0';
	return(i+1);
}

/* some declarations to remove warnings */
void del_addrlist();
int msgtofile();
int queuejob();
int recordmsg();
void rip();

/* send a message */
dosmtpsend(mfp,toargs,nargs,subject)
FILE *mfp;
char *toargs[];
int nargs;
char *subject;
{

	char	smtp_subject[LINELEN],tstring[WRAP_AT + 2],wordwrap[MAXWRAP + 1];
#ifdef OLD
	char	smtp_subject[LINELEN],tstring[LINELEN];
#endif
	char	*tmpnam();
	FILE 	*fpin;
	char	*p;
#ifdef COH386
/*	FILE *tmpfile(); */
	char *tf;
#else
	char 	*tf = "bmXXXXXX";		/* temp file name */
#endif
	int	c;
	int	n;
	long	sequence;
	struct	addr *tolist,*tp;
	register FILE	*tfile;
	time_t	t;

	if (nargs == 0) {
		printf("No recpients\n");
		return 0;
	}
	fpin = mfp;
	if ((tolist = make_tolist(nargs,toargs)) == NULLADDR) {
		printf("Send aborted\n");
		return 0;
	}

	sequence = get_msgid();

	time(&t);
#ifdef COH386
	/* we need to return to this to remove the temp file if we can't
	get mktemp to work */
	if((tf = (char *)malloc(L_tmpnam)) == 0)
		return 1;
	tmpnam(tf);
#else
	mktemp(tf);
#endif
		/* open textfile for write */
	if ((tfile = fopen(tf,"w+")) == NULLFILE) {
		perror(tf);
		del_addrlist(tolist);
		return 1;
	}

	if (!qflag) {
		/* write RFC822-compatible headers using above information */
		fprintf(tfile,"Date: %s",ptime(&t));
		fprintf(tfile,"Message-Id: <%ld@%s>\n",sequence,hostname);
		fprintf(tfile,"From: %s@%s",username,hostname);
		if (fullname != NULLCHAR && *fullname != '\0')
			fprintf(tfile," (%s)",fullname);
		fprintf(tfile,"\n");
		if (replyto != NULLCHAR && *replyto != '\0')
			fprintf(tfile,"Reply-To: %s\n",replyto);
		strcpy(tstring,"To: ");
		for (tp = tolist; tp != NULLADDR; tp = tp->next) {
			strcat(tstring,tp->user);
			if (tp->host != NULLCHAR || *tp->host != '\0') {
				strcat(tstring,"@");
				strcat(tstring,tp->host);
			}
			n = strlen(tstring);
			if (tp->next) {
				if (n > 50) {
					fprintf(tfile,"%s,\n\t",tstring);
					if (tty) printf("%s,\n ",tstring);
					*tstring = '\0';
				} else
					strcat(tstring,", ");
			}
		}
		fprintf(tfile,"%s\n",tstring);
		if (tty) printf("%s\n",tstring);

		*smtp_subject = '\0';
		if (subject == NULLCHAR && tty) {
			/* prompt and get Subject: */
			printf("Subject: ");
			fgets(smtp_subject,LINELEN,stdin);
		} else {
			strcpy(smtp_subject,subject);
			if(tty) printf("Subject: %s\n",smtp_subject);
		}
		fprintf(tfile,"Subject: %s\n",smtp_subject);
		fprintf(tfile,"\n");       /* add empty line as separator */
	}

	if (!tty && fpin == NULLFILE)
		fpin = stdin;

	if (fpin != NULLFILE ) {
		while((c = getc(fpin)) != EOF)
			if(putc(c,tfile) == EOF)
				break;
		if (ferror(tfile)) {
			perror("tmp file");
			(void) fclose(tfile);
			return 1;
		}
	} else {
		/* sending a message not from a file */
		/* copy text from console to the file */
#ifdef UNIX
		printf("\nType message text.  Enter a '.' in column one to end.");
#else
		printf("\nType message text.  Enter a '.' or ctrl/D in column one to end.");
#endif
		printf("\nCommands: ~p - redisplay msg, ~e - invoke editor, ~? - help\n\n");
		*wordwrap = '\0';
#ifdef UNIX
		if(wflag)
			setrawmode();
#endif
		for(;;) {
			/* read line from console  ie stdin */
#ifdef UNIX
			if(wflag)
				lgets(tstring,wordwrap,WRAP_AT);
			else{
				if (fgets(tstring,WRAP_AT + 1,stdin) == NULLCHAR)
					break;  /* we want 80 characters */
				/* see if this line has a c/r at position 80 */
				if(strlen(tstring) == WRAP_AT + 1 && tstring[WRAP_AT] != '\n'){
					ungetc(tstring[WRAP_AT],stdin); /* get it next time */
					tstring[WRAP_AT] = '\0';
				}else
					rip(tstring);
			}
#else
			lgets(tstring,wordwrap,WRAP_AT);
#endif
			if(strcmp(tstring,".") == 0 || tstring[0] == '\004')
				break;
			if (*tstring == '~' ) {
				switch(tstring[1]) {
				case 'p':
					/* Print the message so far */
					fseek(tfile,0L,0);
					while (fgets(tstring,sizeof(tstring),tfile) != NULLCHAR)
						fputs(tstring,stdout);
					break;
				case 'e':
					/* Drop into editor */
					(void) fclose(tfile);
					if (editor == NULLCHAR) {
						printf("No editor defined\n");
						tfile = fopen(tf,"a+");
#ifdef UNIX
						if(wflag)
							setrawmode();
#endif
						break;
					}
					sprintf(tstring,"%s %s",editor,tf);
					/* call editor to enter message text */
#ifdef UNIX
					if(wflag)
						setcookedmode();
#endif
					if (system(tstring))
#ifdef UNIX	/* vi reports all kinds of errors in return value */
					;
#else
						printf("unable to invoke editor\n");
#endif
					tfile = fopen(tf,"a+");
					break;
				case 'q':
					(void) fclose(tfile);
					(void) unlink(tf);
					printf("Abort\n");
#ifdef UNIX
					if(wflag)
						setcookedmode();
#endif
					return 0;
				case 'r':
					{
						FILE  *infl;
						p = &tstring[2];
						while(*p == ' ' || *p == '\t')
							p++;
						if (*p == '\0')
							printf("No file name specified\n");
						else
							if((infl = fopen(p,"r")) == NULLFILE)
								printf("No such file\n");
							else {
								printf("Reading file %s\n",p);
								while((c = getc(infl)) != EOF)
									if(putc(c,tfile) == EOF)
										break;
								if (ferror(tfile)) {
									perror("tmp file");
									(void) fclose(tfile);
#ifdef UNIX
									if(wflag)
										setcookedmode();
#endif
									return 1;
								}
								(void) fclose(infl);
							}

						break;
					}
				case 'm':
					{
						int	msg;
						p = &tstring[2];
						while(*p == ' ' || *p == '\t')
							p++;
						if (*p == '\0')
							msg = current;
						else
							msg = atoi(p);
						if (mfile == NULLFILE || msg < 1 || msg > nmsgs)
							printf("no such message\n");
						else {
							printf("Inserting message %d\n",msg);
							msgtofile(msg,tfile,0);
						}
						break;
					}
				case '~':
					fprintf(tfile,"%s\n",&tstring[1]);
					break;
				case '?':
					printf("~e - Invoke Editor\n");
					printf("~p - Display message buffer\n");
					printf("~q - Abort this message\n");
					printf("~r file - Read file into buffer\n");
					printf("~m msg - message into buffer\n");
					printf("~~ - Enter a ~ into message\n");
					break;
				default:
					printf("Unknown ~ escape. ~? for help\n");
				}
				printf("(continue)\n");
			} else
				fprintf(tfile,"%s\n",tstring);
		}
#ifdef UNIX
		if(wflag)
			setcookedmode();
#endif
		printf("EOF\n");
	}
	queuejob(tfile,tolist,sequence);
	recordmsg(tfile,tolist->user); 	/* save copy for sender */
	(void) fclose(tfile);
	del_addrlist(tolist);
	(void) unlink(tf);
	return 0;
}

/* forward a message in its orginal form */
bouncemsg(mfp,toargs,nargs)
FILE *mfp;
char *toargs[];
int nargs;
{
	struct	addr *list;
	long sequence;
	if (nargs == 0) {
		printf("No recpients\n");
	} else if ((list = make_tolist(nargs,toargs)) == NULLADDR) {
		printf("Bad to list\n");
	} else {
		sequence = get_msgid();  /* K5JB */
		queuejob(mfp,list,sequence);
		del_addrlist(list);
	}
	return 0;
}

/* Return Date/Time in Arpanet format in passed string */
/* Print out the time and date field as
 *		"DAY day MONTH year hh:mm:ss ZONE"
 */
char *
ptime(t)
long *t;
{
	register struct tm *ltm;
	struct tm *localtime();
	static char tz[4];
	static char str[40];
	extern char *getenv();
	extern char timez[];	/* added to Unix, enables zone command in .bmrc */
	char *p;
	static char *days[7] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };

	static char *months[12] = {
		"Jan","Feb","Mar","Apr","May","Jun",
		"Jul","Aug","Sep","Oct","Nov","Dec" };

	/* Read the system time */
	ltm = localtime(t);

	if (*tz == '\0'){	/* did some work to prevent array over-writes */
		if(timez[0] == 'x')   /* nothing read from bm.rc - added to Unix */
			if ((p = getenv("TZ")) == NULLCHAR)
				strcpy(timez,"GMT");
			else
				strncpy(timez,p,7);
		p = timez;
		if(ltm->tm_isdst && strlen(timez) >= 7)	/* TZ from environ will */
		/* be 8 and if zone command is in .bmrc, CST6CDT results in 7 */
			p += 4;
		strncpy(tz,p,3);
	}
#ifdef OLD
	if (*tz == '\0')
		if ((p = getenv("TZ")) == NULLCHAR)
			strcpy(tz,"GMT");
		else{
			if(ltm->tm_isdst)
				p += 4;
			strncpy(tz,p,3);
		}
#endif



	/* rfc 822 format */
	sprintf(str,"%s, %.2d %s %02d %02d:%02d:%02d %.3s\n",
		days[ltm->tm_wday],
		ltm->tm_mday,
		months[ltm->tm_mon],
		ltm->tm_year,
		ltm->tm_hour,
		ltm->tm_min,
		ltm->tm_sec,
		tz);
	return(str);
}

/* save copy in the record file */
int
recordmsg(dfile,to)
FILE *dfile;
char *to;
{
	register int c;
	FILE 	*fp;
	time_t	t;

	if (record == NULLCHAR)
		return 1;
	fseek(dfile,0L,0);
	if ((fp = fopen(record,"a")) == NULLFILE) {
		printf("Unable to append to %s\n",record);
	} else {
		(void) time(&t);
		fprintf(fp,"From %s %s",to,ctime(&t));
		while((c = getc(dfile)) != EOF)
			if(putc(c,fp) == EOF)
				break;
		if (ferror(fp)) {
			(void) fclose(fp);
			return 1;
		}
		(void) fclose(fp);
	}
	return 0;
}

/* place a mail job in the outbound queue */
/* changed method of getting sequence ID to prevent doubling - K5JB */
int
queuejob(dfile,tolist,sequence)
FILE *dfile;
struct addr *tolist;
long sequence;
{
	FILE *fp;
	char tmpstring[50];
	register struct addr *tp,*sp;
	char prefix[9];
	int c;
	long id = 0;

	for (tp = tolist; tp != NULLADDR; tp = tp->next) {
		if (tp->sent)
			continue;
		fseek(dfile,0L,0);
		if(id == 0)
			id = sequence;
			else
				id = get_msgid();  /* for subsequent addrs K5JB */
		sprintf(prefix,"%ld",id);
		(void) mlock(mqueue,prefix);
		sprintf(tmpstring,"%s/%s.txt",mqueue,prefix);
#ifdef MSDOS
		if((fp = fopen(tmpstring,"wt")) == NULLFILE){
#else
		if((fp = fopen(tmpstring,"w")) == NULLFILE) {
#endif
			printf("unable to open %s\n",tmpstring);
			(void) rmlock(mqueue,prefix);
			return 1;
		}
		while((c = getc(dfile)) != EOF)
			if(putc(c,fp) == EOF)
				break;
		if (ferror(fp)) {
			(void) fclose(fp);
			(void) rmlock(mqueue,prefix);
			return 1;
		}
		(void) fclose(fp);
		sprintf(tmpstring,"%s/%s.wrk",mqueue,prefix);
		if((fp = fopen(tmpstring,"w")) == NULLFILE) {
			(void) rmlock(mqueue,prefix);
			return 1;
		}
		fprintf(fp,"%s\n%s@%s\n",tp->host,username,hostname);
		fprintf(fp,"%s@%s\n",tp->user,tp->host);
		tp->sent++;
		/* find and other addresses to the same host */
		for (sp = tp->next; sp != NULLADDR; sp = sp->next) {
			if (sp->sent)
				continue;
			if (strcmp(tp->host,sp->host) == 0) {
				fprintf(fp,"%s@%s\n",sp->user,sp->host);
				sp->sent++;
			}
		}
		(void) fclose(fp);
		(void) rmlock(mqueue,prefix);
	}
	return 0;
}

#define SKIPWORD(X) while(*X && *X!=' ' && *X!='\t' && *X!='\n' && *X!= ',') X++;
#define SKIPSPACE(X) while(*X ==' ' || *X =='\t' || *X =='\n' || *X == ',') X++;

/* check for and alias and expand alais into a address list */
struct addr *
expandalias(head, user)
struct addr **head;
char *user;
{
	FILE *fp;
	register char *s,*p,*h;
	int inalias;
	extern char *alias;
	struct addr *tp;
	char buf[LINELEN];

	fp = fopen(alias, "r");

		/* no alias file found */
	if (fp == NULLFILE)
		return addrecip(head, user, hostname);

	inalias = 0;
	while (fgets(buf,LINELEN,fp) != NULLCHAR) {
		p = buf;
		if ( *p == '#' || *p == '\0')
			continue;
		rip(p);

		/* if not in an matching entry skip continuation lines */
		if (!inalias && isspace(*p))
			continue;

		/* when processing an active alias check for a continuation */
		if (inalias) {
			if (!isspace(*p))
				break;	/* done */
		} else {
			s = p;
			SKIPWORD(p);
			*p++ = '\0';	/* end the alias name */
			if (strcmp(s,user) != 0)
				continue;	/* no match go on */
			inalias = 1;
		}

		/* process the recipients on the alias line */
		SKIPSPACE(p);
		while(*p != '\0' && *p != '#') {
			s = p;
			SKIPWORD(p);
			if (*p != '\0')
				*p++ = '\0';
			/* find hostname */
			if ((h = strchr(s,'@')) != NULLCHAR)
				*h++ = '\0';
			else
				h = hostname;
			tp = addrecip(head,s,h);
			SKIPSPACE(p);
		}
	}
	(void) fclose(fp);

	if (inalias)	/* found and processed and alias. */
		return tp;

	/* no alias found treat as a local address */
	return addrecip(head, user, hostname);
}

/* convert a arg list to an list of address structures */
struct addr *
make_tolist(argc,argv)
int argc;
char *argv[];
{
	struct addr *tolist, *tp;
	char *user, *host;
	int i;
	tolist = NULLADDR;
	for (i = 0; i < argc; i++) {
		user = argv[i];
		if ((host = strchr(user ,'@')) != NULLCHAR) {
			*host++ = '\0';
			/* if it matches our host name */
			if (stricmp(host,hostname) == 0)
				host = NULLCHAR;
		}

		if (host == NULLCHAR)	/* a local address */
			tp = expandalias(&tolist,user);
		else		/* a remote address */
			tp = addrecip(&tolist, user, host);

		if (tp == NULLADDR) {
			printf("Out of memory\n");
			del_addrlist(tolist);
			return NULLADDR;
		}
	}
	return tolist;
}

/* delete a list of mail addresses */
void
del_addrlist(list)
struct addr *list;
{
	struct addr *tp, *tp1;;
	for (tp = list; tp != NULLADDR; tp = tp1) {
		tp1 = tp->next;
		if (tp->user != NULLCHAR);
			free(tp->user);
		if (tp->host != NULLCHAR);
			free(tp->host);
		(void) free((char *)tp);
	}
}

/* add an address to the from of the list pointed to by head
** return NULLADDR if out of memory.
*/
struct addr *
addrecip(head,user,host)
struct addr **head;
char *user, *host;
{
	register struct addr *tp;

	tp = (struct addr *)calloc(1,sizeof(struct addr));
	if (tp == NULLADDR)
		return NULLADDR;

	tp->next = NULLADDR;

	/* allocate storage for the user's login */
	if ((tp->user = (char *)malloc((unsigned)strlen(user)+1)) == NULLCHAR) {
		(void) free((char *)tp);
		return NULLADDR;
	}
	strcpy(tp->user,user);

	/* allocate storage for the host name */
	if (host != NULLCHAR)
		if ((tp->host = (char *)malloc((unsigned)strlen(host)+1)) == NULLCHAR) {
			(void) free(tp->user);
			(void) free((char *)tp);
			return NULLADDR;
		}
	strcpy(tp->host,host);

	/* add entry to front of existing list */
	if (*head == NULLADDR)
		*head = tp;
	else {
		tp->next = *head;
		*head = tp;
	}
	return tp;

}

