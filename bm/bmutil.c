/* bmutil.c */
/* setrawmode and setcookedmode were causing characters to be
 * scrambled on my terminal so they were undefed out for UNIX.
 * (fixed this 12/16/91, while doing some other cosmetic things.)
 * But, cooked and raw only used in Unix for wordwrap and haktc().
 * took out the screen_clear while I was at it since there is
 * an empty function for that...don't think I want screen clearing
 * cutsies anyway - K5JB
 * Fixed some file reading errors 1/94 and cleaned up some more while
 * adding Ctrl-Z filter in 6/94 - K5JB
 */

#include <stdio.h>
#include <ctype.h>
#include <time.h>

#ifdef	_OSK
#include <modes.h>
#include <strings.h>
#define strchr index
#define strrchr rindex
#else
#include <string.h>
#endif

#if	(defined(UNIX) || defined(MICROSOFT)) && !defined(_OSK)
#include	<sys/types.h>
#endif
#if	(defined(UNIX) || defined(MICROSOFT) || defined(__TURBOC__)) && !defined(_OSK)
#include	<sys/stat.h>
#endif
#if defined(AZTEC) || defined(_OSK)
#include <stat.h>
#endif
#ifndef	_OSK
#include <fcntl.h>
#endif
#ifdef __TURBOC__
#include <conio.h>
#include <stdlib.h>
#include <io.h>
#endif

#include "bm.h"
#include "header.h"
#include "config.h"

char *getname();
static unsigned long	mboxsize;
static int	anyread;

#ifdef SETVBUF
#define		MYBUF	4096
char	*inbuf = NULLCHAR;	/* the stdio buffer for the mail file */
char	*outbuf = NULLCHAR;	/* the stdio file io buffer for the temp file */
#endif

int
initnotes()
{
	FILE	*tmpfile();
	FILE	*ifile;
	register struct	let *cmsg;
	int 	i, ret;
	struct stat mstat;

	if (!stat(mfilename,&mstat))
		mboxsize = mstat.st_size;
#ifdef MSDOS	/* to deal with files containing ^Z -- 061894 */
	if ((ifile = fopen(mfilename,"rb")) == NULLFILE)
#else
	if ((ifile = fopen(mfilename,"r")) == NULLFILE)
#endif
	{
		printf(nomail);
		mfile = NULLFILE;
		return 0;
	}
#ifdef	SETVBUF
	if (inbuf == NULLCHAR)
		inbuf = (char *)malloc(MYBUF);
	setvbuf(ifile, inbuf, _IOFBF, MYBUF);
#endif
	if ((mfile = tmpfile()) == NULLFILE) {
		printf("Unable to create tmp file\n");
		(void) fclose(ifile);
		return -1;
	}
#ifdef SETVBUF
	if (outbuf == NULLCHAR)
		outbuf = (char *)malloc(MYBUF);
	setvbuf(mfile, outbuf, _IOFBF, MYBUF);
#endif
	nmsgs = 0;
	current = 0;
	change = 0;
	newmsgs = 0;
	anyread = 0;
	ret = readnotes(ifile);
	(void) fclose(ifile);
	if (ret != 0)
		return -1;
#ifdef SETVBUF
	if (inbuf != NULLCHAR) {
		(void) free(inbuf);
		inbuf = NULLCHAR;
	}
#endif
	for (cmsg = &mbox[1],i = 1; i <= nmsgs; i++, cmsg++)
		if ((cmsg->status & READ) == 0) {
			newmsgs++;
			if (current == 0)
				current = i;
		}
	/* start at one if no new messages */
	if (current == 0)
		current++;

	return 0;

}

int
haktc(percent)
int percent;
{
	int c;
	printf("- q to quit, any other key for more ");
	if(percent)
		printf("(%d%%) - ",percent);
		else
		printf("- ");
#ifdef UNIX
	fflush(stdout);
#endif
#ifdef MSDOS
	while(!kbhit())
	;
	c = (char)getch();
#else
	/* getch() will wait until a character is hit */
	c = (char)getch();
#endif
	/* getch will not  echo whatever key was hit, so */
	printf("%c\n", c == '\r' || c == '\n' ? '\0' : c);
	if( c == EOF || c == 'q')
		return(-1);
#ifdef CLEAR
	screen_clear();	/* K5JB */
#endif
	return(0);
}

#ifdef MSDOS
/* like fgets, except does binary read so not stopped by ^Z in file.
 * s must have n+1 char spaces, n must be greater than zero, and stream
 * must be a file opened in binary mode - K5JB
 */
char *
bfgets(s,n,stream)
char *s;
int n;
FILE *stream;
{
	int temp,linedone;
	char *lineptr;

	s[--n] = '\0';
	for(;;){
		linedone = FALSE;
		lineptr = s;
		while(n--){
			temp = getc(stream);
			switch(temp){
				case '\n':
					*lineptr++ = (char)temp;
				case EOF:
					*lineptr = '\0';
					linedone = TRUE;
					break;
				case 26:
				case 13:
					n++;
					break;
				default:
					*lineptr++ = (char)temp;
					break;
			}
			if(linedone)
				break;
		}
		if(temp == EOF)
			return(NULLCHAR);
		else
			return(s);
	}
}
#endif

/* readnotes assumes that ifile is pointing to the first
 * message that needs to be read.  For initial reads of a
 * notesfile, this will be the beginning of the file.  For
 * rereads when new mail arrives, it will be the first new
 * message.
 * 011495 rearranged because this was tacking extra line to end of
 * mailfile, 061894 added bfgets() to handle mail with imbedded Ctrl-Z.
 * File is opened in binary and bfgets fixes end of lines as well as
 * zapping Ctrl-Zs.
 */
readnotes(ifile)
FILE *ifile ;
{
	char 	tstring[LINELEN];
	long	cpos;
	register struct	let *cmsg;
	register char *line;
	long	ftell();

	cmsg = (struct let *)0;
	line = tstring;
	for(;;){
#ifdef MSDOS
		if(bfgets(line,LINELEN,ifile) == NULLCHAR)
#else
		if(fgets(line,LINELEN,ifile) == NULLCHAR || feof(ifile))
#endif
			break;	/* 011494 was tacking extra line to end of mailfile */
		/* scan for begining of a message */
		if(strncmp(line,"From ",5) == 0) {
			cpos = ftell(mfile);
			fputs(line,mfile);
			if (nmsgs == maxlet) {
				printf("Mail box full: > %d messages\n",maxlet);
#if defined(_OSK) && !defined(_UCC)
				tmpclose(mfile);
#else
				(void) fclose(mfile);
#endif
				return -1;
			}
			nmsgs++;
			cmsg = &mbox[nmsgs];
			cmsg->start = cpos;
			cmsg->status = 0;
			cmsg->size = strlen(line);
			for(;;){
#ifdef MSDOS
				if(bfgets(line,LINELEN,ifile) == NULLCHAR)
#else
				if (fgets(line,LINELEN,ifile) == NULLCHAR || feof(ifile))
#endif
					break;
				if (*line == '\n') { /* done header part */
					cmsg->size++;
					putc(*line, mfile);
					break;
				}
				if (htype(line) == STATUS) {
					if (line[8] == 'R')
						cmsg->status |= READ;
					continue;
				}
				cmsg->size += strlen(line);
				if (fputs(line,mfile) == EOF) {
					perror("tmp file");
#if defined(_OSK) && !defined(_UCC)
					tmpclose(mfile);
#else
					(void) fclose(mfile);
#endif
					return -1;
				}

			}
		} else if (cmsg) {
			cmsg->size += strlen(line);
			fputs(line,mfile);
		}
	}
	return 0;
}

/* list headers of a notesfile a message */
void
listnotes()
{
	register struct	let *cmsg;
	register char	*p, *s;
	char	smtp_date[SLINELEN];
	char	smtp_from[SLINELEN];
	char	smtp_subject[SLINELEN];
	char	tstring[LINELEN];
	int	i;
	int	lines;
	long	size;
	static int timesrun;

	if (mfile == NULLFILE)
		return;

#ifdef CLEAR
	screen_clear();		/* K5JB */
#endif
	printf("Mailbox %s - %d messages %d new\n\n",mfilename,nmsgs,newmsgs);
#if defined(MSDOS) && defined(DVREPORT)
	lines = timesrun++ ? 2 : 7;	/* Forgot initial banner v.1n2 */
#else
	lines = timesrun++ ? 2 : 5;
#endif
	for (cmsg = &mbox[1],i = 1; i <= nmsgs; i++, cmsg++) {
		*smtp_date = '\0';
		*smtp_from = '\0';
		*smtp_subject = '\0';
		fseek(mfile,cmsg->start,0);
		size = cmsg->size;
		while(size > 0){
			if(fgets(tstring,sizeof(tstring),mfile) == NULLCHAR)
				break;
			if (*tstring == '\n')	/* end of header */
				break;
			size -= strlen(tstring);
			rip(tstring);
			/* handle continuation later */
			if (*tstring == ' '|| *tstring == '\t')
				continue;
			switch(htype(tstring)) {
			case FROM:
				if((p = getname(tstring)) == NULLCHAR) {
					p = &tstring[6];
					while(*p && *p != ' ' && *p != '(')
						p++;
					*p = '\0';
					p = &tstring[6];
				}
				sprintf(smtp_from,"%.30s",p);
				break;
			case SUBJECT:
				sprintf(smtp_subject,"%.34s",&tstring[9]);
				break;
			case DATE:
				if ((p = strchr(tstring,',')) == NULLCHAR)
					p = &tstring[6];
				else
					p++;
				/* skip spaces */
				while (*p == ' ') p++;
				if(strlen(p) < 15)	/* was 17, missed if no seconds - K5JB */
					break; 	/* not a valid length */
				s = smtp_date;
				/* copy day */
				if (atoi(p) < 10 && *p != '0') {
					*s++ = ' ';
				} else
					*s++ = *p++;
				*s++ = *p++;

				*s++ = ' ';
				while (*p == ' ')
					p++;
				/* copy month */
				*s++ = *p++;
				*s++ = *p++;
				*s++ = *p++;
				while (*p == ' ')
					p++;
				/* skip year */
				while (isdigit(*p))
					p++;
				/* copy time */
				*s++ = *p++;	/* space */
				/* Wonderful ELM will do 1:00 - K5JB */
				if (atoi(p) < 10 && *p != '0')
					*s++ = ' ';	/* Don't put a '0' in there */
				else				/* Show the booger */
					*s++ = *p++;	/* hour */
				*s++ = *p++;
				*s++ = *p++;	/* : */
				*s++ = *p++;	/* min */
				*s++ = *p++;
				*s = '\0';
				break;
			case NOHEADER:
				break;
			}
		}
		printf("%c%c%c%3d %-27.27s %-12.12s %5ld %.25s\n",
		    (i == current ? '>' : ' '),
		    (cmsg->status & DELETE ? 'D' : ' '),
		    (cmsg->status & READ ? 'Y' : 'N'),
		    i, smtp_from, smtp_date,
		    cmsg->size, smtp_subject);
		if ((++lines % (MAXROWS - 1)) == 0) {
			if(haktc(0) == -1)
				break;
			lines = 0;
		}
	}
}

/*  save msg on stream - if noheader set don't output the header */
void	/* nobody looked at return */
msgtofile(msg,tfile,noheader)
int msg;
FILE *tfile;   /* already open for write */
int noheader;
{
	char	tstring[LINELEN];
	long 	size;
#ifdef QUOTING
	extern int quoting;
#endif
	if (mfile == NULLFILE) {
		printf(nomail);
		return;
	}
	fseek(mfile,mbox[msg].start,0);
	size = mbox[msg].size;
	if ((mbox[msg].status & READ) == 0) {
		mbox[msg].status |= READ;
		change = 1;
	}

	if (noheader) {
		/* skip header */
		while (size > 0) {
			if(fgets(tstring,sizeof(tstring),mfile) == NULLCHAR)
				break;
			size -= strlen(tstring);
			if (*tstring == '\n')
				break;
		}
	}
	while (size > 0) {
		if(fgets(tstring,sizeof(tstring),mfile) == NULLCHAR)
			break;
		size -= strlen(tstring);
#ifdef QUOTING	/* may add another pp directive if I like this */
		fprintf(tfile,"%s%s",quoting ? ">" : "",tstring);	/* add quoting */
#else
		fputs(tstring,tfile);
#endif
		if (ferror(tfile)) {
			printf("Error writing mail file\n");
			(void) fclose(tfile);
			return;
		}
	}
#ifdef QUOTING
	quoting = 0;
#endif
}

/*  delmsg - delete message in current notesfile */
void
delmsg(msg)
int msg;
{
	if (mfile == NULLFILE) {
		printf(nomail);
		return;
	}
	mbox[msg].status |= DELETE;
	change = 1;
}

/*  reply - to a message  */
void
reply(msg)
int msg;
{
	char 	to[SLINELEN];
	char	subject[LINELEN];
	char	tstring[LINELEN];
	char 	*p,*s;
	char	*toarg[1];
	long	size;

	if (mfile == NULLFILE) {
		printf(nomail);
		return;
	}
	*to = '\0';
	*subject = '\0';
	fseek(mfile,mbox[msg].start,0);
	size = mbox[msg].size;
	while (size > 0) {
		if(fgets(tstring,sizeof(tstring),mfile) == NULLCHAR);
		size -= strlen(tstring);
		if (*tstring == '\n') 	/* end of header */
			break;
		rip(tstring);
		if ((*to == '\0' && htype(tstring) == FROM)
		    || htype(tstring) == REPLYTO) {
			s = getname(tstring);
			if (s == NULLCHAR) {
				p = strchr(tstring,':');
				p += 2;
				s = p;
				while(*p && *p != ' ' && *p != '(')
					p++;
				*p = '\0';
			}
			*to = '\0';
			strncat(to,s,sizeof(to));
		} else if (htype(tstring) == SUBJECT) {
			if (!strncmp(&tstring[9], "Re:", 3) ||
				!strncmp(&tstring[9], "re:", 3))  /* Has Re: or re: */
				sprintf(subject,"%s",&tstring[9]) ;
			else	/* let's add one */
				sprintf(subject,"Re: %s",&tstring[9]);
		}
	}
	if (*to == '\0')
		printf("No reply address in message\n");
	else {
		toarg[0] = to;
		dosmtpsend(NULLFILE,toarg,1,subject);
	}
}


/* close the temp file while coping mail back to the mailbox */
int
closenotes()
{
	register struct	let *cmsg;
	register char *line;
	char tstring[LINELEN];
	long size;
	int i;
	int ret;
	FILE	*nfile;
	struct stat mstat;

	if (mfile == NULLFILE)
		return 0;
	if (!change) {
#if defined(_OSK) && !defined(_UCC)
		tmpclose(mfile);
#else
		(void) fclose(mfile);
#endif
		return 0;
	}
	line = tstring;
	fseek(mfile,0L,2);
	if (isnewmail()) {
#ifdef MSDOS	/* 061894 */
		if ((nfile = fopen(mfilename,"rb")) == NULLFILE)
#else
		if ((nfile = fopen(mfilename,"r")) == NULLFILE)
#endif
			perror(mfilename);
		else {
			/* seek to end of old msgs */
			fseek(nfile,mboxsize,0);
			/* seek to end of tempfile */
			fseek(mfile,0L,2);
			ret = readnotes(nfile);   /* get the new mail */
			(void) fclose(nfile);
			if (ret != 0) {
				printf("Error updating mail file\n");
				return -1;
			}
		}
	}

	if ((nfile = fopen(mfilename,"w")) == NULLFILE) {
		printf("Unable to open %s\n",mfilename);
		return -1;
	}
	/* copy tmp file back to notes file */
	for (cmsg = &mbox[1],i = 1; i <= nmsgs; i++, cmsg++) {
		fseek(mfile,cmsg->start,0);
		size = cmsg->size;
		if ((cmsg->status & DELETE))
			continue;
		/* copy the header */
		while (size > 0) {
			if(fgets(line,LINELEN,mfile) == NULLCHAR)
				break;
			size -= strlen(line);
			if (*line == '\n') {
				if ((cmsg->status & READ) != 0)
					fprintf(nfile,"Status: R\n");
				fprintf(nfile,"\n");
				break;
			}
			fputs(line,nfile);
		}
		while (size > 0) {
			if(fgets(line,LINELEN,mfile) == NULLCHAR)
				break;
			fputs(line,nfile);
			size -= strlen(line);
			if (ferror(nfile)) {
				printf("Error writing mail file\n");
				(void) fclose(nfile);
#if defined(_OSK) && !defined(_UCC)
				tmpclose(mfile);
#else
				(void) fclose(mfile);
#endif
				return -1;
			}
		}
	}
	nmsgs = 0;
	(void) fclose(nfile);
#if defined(_OSK) && !defined(_UCC)
	tmpclose(mfile);
#else
	(void) fclose(mfile);
#endif
	mfile = NULLFILE;

	/* remove a zero length file */
	if (stat(mfilename,&mstat) == 0  && mstat.st_size == 0)
		(void) unlink(mfilename);
	return 0;
}

/* get a message id from the sequence file */
long
get_msgid()
{
	char sfilename[SLINELEN];
	char s[20];
	long sequence = 0L;
	FILE *sfile;
	long atol();

	sprintf(sfilename,"%s/sequence.seq", mqueue);
	sfile = fopen(sfilename,"r");

	/* if sequence file exists, get the value, otherwise set it */
	if (sfile != NULLFILE) {
		fgets(s,sizeof(s),sfile);
		sequence = atol(s);
	/* Keep it in range of and 8 digit number to use for dos name prefix. */
		if (sequence < 0L || sequence > 99999999L )
			sequence = 0L;
		(void) fclose(sfile);
	}

	/* increment sequence number, and write to sequence file */
	sfile = fopen(sfilename,"w");
	fprintf(sfile,"%ld",++sequence);
	(void) fclose(sfile);
	return sequence;
}

/* Given a string of the form <user@host>, extract the part inside the
 * brackets and return a pointer to it.
 */
char *
getname(cp)
register char *cp;
{
	char *cp1;

	if((cp = strchr(cp,'<')) == NULLCHAR)
		return NULLCHAR;
	cp++;	/* cp -> first char of name */
	if((cp1 = strchr(cp,'>')) == NULLCHAR)
		return NULLCHAR;
	*cp1 = '\0';
	return cp;
}

/* create mail lockfile */
int
mlock(dir,id)
char *dir;
char *id;
{
	char lockname[SLINELEN];
	int fd;
	/* Try to create the lock file in an atomic operation */
	sprintf(lockname,"%s/%.8s.lck",dir,id);
#ifdef	_OSK
	if((fd = create(lockname,_WRITE,S_IWRITE+S_IREAD)) == -1)
#else
	if((fd = open(lockname, O_WRONLY|O_EXCL|O_CREAT,0600)) == -1)
#endif
		return -1;
	(void) close(fd);
	return 0;
}

/* remove mail lockfile */
void
rmlock(dir,id)
char *dir;
char *id;
{
	char lockname[SLINELEN];
	sprintf(lockname,"%s/%.8s.lck",dir,id);
	(void) unlink(lockname);
}

/* parse a line into argv array. Return argc */
int
parse(line,argv,maxargs)
register char *line;
char *argv[];
int maxargs;
{
	int argc;
	int qflag;
	register char *cp;

	for(argc = 0; argc < maxargs; argc++)
		argv[argc] = NULLCHAR;

	for(argc = 0; argc < maxargs;){
		qflag = 0;
		/* Skip leading white space */
		while(*line == ' ' || *line == '\t')
			line++;
		if(*line == '\0')
			break;
		/* Check for quoted token */
		if(*line == '"'){
			line++;	/* Suppress quote */
			qflag = 1;
		}
		argv[argc++] = line;	/* Beginning of token */
		/* Find terminating delimiter */
		if(qflag){
			/* Find quote, it must be present */
			if((line = strchr(line,'"')) == NULLCHAR){
				return -1;
			}
			*line++ = '\0';
		} else {
			/* Find space or tab. If not present,
			 * then we've already found the last
			 * token.
			 */
			if((cp = strchr(line,' ')) == NULLCHAR
			    && (cp = strchr(line,'\t')) == NULLCHAR){
				break;
			}
			*cp++ = '\0';
			line = cp;
		}
	}
	return argc;
}

lockit()
{
	char line[8];
	while(mlock(maildir,notename)) {
		printf("Mail file is busy, Abort or Retry ? ");
		fgets(line,8,stdin);
		if (*line == 'A' || *line == 'a') {
			if ( mfile != NULLFILE)
#if defined(_OSK) && !defined(_UCC)
				tmpclose(mfile);
#else
				(void) fclose(mfile);
#endif
			mfile = NULLFILE;
			return 1;
		}
	}
	return 0;
}

/* print the next message or the current on of new */
void
printnext()
{
	if (mfile == NULLFILE)
		return;
	if ((mbox[current].status & READ) != 0) {
		if (current == 1 && anyread == 0)
			;
		else if (current < nmsgs) {
			current++;
		} else {
			printf("Last message\n");
			return;
		}
	}
	displaymsg(current);
	anyread = 1;
}

/*  display message on the crt given msg number */
void
displaymsg(msg)
int msg;
{
	register int c;
	register int col;
	char	buf[MAXCOL];		/* line buffer; reduced by 2, don't need more */
	int	lines;				/* than 80 - K5JB */
	int	cnt;
	long 	tsize, size;

	if (mfile == NULLFILE) {
		printf(nomail);
		return;
	}
	if( msg < 0 || msg > nmsgs) {
		printf(badmsg,msg);
		return;
	}
#ifdef CLEAR
	screen_clear();		/* K5JB */
#endif
	fseek(mfile,mbox[msg].start,0);
	size = mbox[msg].size;
	tsize = size;

	printf("Message #%d %s\n",
		msg, mbox[msg].status & DELETE ? "[Deleted]" : "");
	if ((mbox[msg].status & READ) == 0) {
		mbox[msg].status |= READ;
		change = 1;
	}
	lines = 1;
	col = 0;
	while (!feof(mfile) && size > 0){
		for (col = 0;  col < MAXCOL;) {	/* MAXCOL is 80 */
			c = getc(mfile);
			size--;
			if (feof(mfile) || size == 0)	/* end this line */
				break;
			if (c == '\t') {
				cnt = col + 4 - (col & 3);	/* cute trick! But I cut it in half */
				if (cnt >= MAXCOL - 1)	/* end this line */
					break;
				while (col < cnt)
					buf[col++] = ' ';
			} else
				if (c == '\n')	/* at col 79 we'll tolerate a \n, but... */
					break;
				else
					if(col < MAXCOL - 1)
						buf[col++] = c; /* max is col[78], 79 chars */
					else{
						ungetc(c,mfile);	/* ...nothing else if at pos 80 */
						size++;	/* fixed in version 1n2 */
						break;
					}
		}
		buf[col] = '\0';
#ifdef OLD
		puts(buf);	/* only place puts() appears in bm */
#else
		printf("%s\n",buf);
#endif
		col = 0;
		if ((++lines == (MAXROWS-1))) {
			if(haktc((tsize-size)*100/tsize) == -1)
				break;
			lines = 0;
		}
	}
}

/* list jobs waiting to be sent in the mqueue */
void
listqueue()
{
	char tstring[80];
	char workfile[80];
	char line[16]; /* reduced from 20 */
	char host[SLINELEN];
	char to[SLINELEN];
	char from[SLINELEN];
	char *p;
	char	status;
	struct stat stbuf;
	struct tm *tminfo, *localtime();
	FILE *fp;

	printf("S     Job    Size Date  Time  Host                 From\n");
	sprintf(workfile,"%s/%s",mqueue,WORK);
	filedir(workfile,0,line);
	while(line[0] != '\0') {
		sprintf(tstring,"%s/%s",mqueue,line);
		if ((fp = fopen(tstring,"r")) == NULLFILE) {
			perror(tstring);
			continue;
		}
		if ((p = strrchr(line,'.')) != NULLCHAR)
			*p = '\0';
		sprintf(tstring,"%s/%s.lck",mqueue,line);
		if (access(tstring,0))
			status = ' ';
		else
			status = 'L';
		sprintf(tstring,"%s/%s.txt",mqueue,line);
		stat(tstring,&stbuf);
		tminfo = localtime(&stbuf.st_ctime);
		fgets(host,sizeof(host),fp);
		rip(host);
		fgets(from,sizeof(from),fp);
		rip(from);
		printf("%c %7s %7ld %02d/%02d %02d:%02d %-20s %s\n      ",
			status, line, stbuf.st_size,
			tminfo ->tm_mon+1,
			tminfo->tm_mday,
			tminfo->tm_hour,
			tminfo->tm_min,
			host,from);
		while (fgets(to,sizeof(to),fp) != NULLCHAR) {
			rip(to);
			printf("%s ",to);
		}
		printf("\n");
		(void) fclose(fp);
		filedir(workfile,1,line);
	}
}

/* kill a job in the mqueue */
void
killjob(j)
char *j;
{
	char s[SLINELEN];
	char tbuf[8];
	char *p;
	sprintf(s,"%s/%s.lck",mqueue,j);
	p = strrchr(s,'.');
	if (!access(s,0)) {
		printf("Warning Job %s is locked by SMTP. Remove (y/n)? ",j);
		fgets(tbuf,8,stdin);
		if (*tbuf != 'y')
			return;
		(void) unlink(s);
	}
	strcpy(p,".wrk");
	if (unlink(s))
		printf("Job id %s not found\n",j);
	strcpy(p,".txt");
	(void) unlink(s);
}

/* check the current mailbox to see if new mail has arrived.
* checks to see if the file has increased in size.
* returns true if new mail has arrived.
*/
isnewmail()
{
	struct stat mstat;
	if (mfile == NULLFILE)
		return 0;
	if (stat(mfilename,&mstat))
		printf("unable to stat %s\n",mfilename);
	else if (mstat.st_size > mboxsize)
			return 1;
	return 0;
}
