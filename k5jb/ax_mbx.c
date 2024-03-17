/* ax_mbx.c, non xobbs part.  Merge with full ax_mbx.c before dist-
 * ribution.  This version has some of the new mbox features.  Code is
 * Unix compatible.  K5JB
 * 3/3/90 added check for disconnected link during a download and stifled
 * use of memory a bit during that process - K5JB
 * 6/6/91 added time out timers to AX25 and Netrom sessions.  Set it at
 * 10 minutes.
 * 6/25/91 Version with selective file reading, beginning with v.i.9
 * l lists message headers, ls shows directories, r [n] reads message n,
 * k [n] kills message n.  If only one message, r or k can be used without
 * a number.  On connect, maximum of last 5 messages shown.
 * 6/29/91 Made change to ax25subr.c to trap double freeing of axp in
 * del_ax25 which occurs on disconnection during download over ax25 link.
 * It didn't seem to happen with netrom link.  Rearranged handling of abort
 * function for download.  (Later undid this)
 * 7/11/91 Revised file sending to use transmit upcall, added prompting
 * to the timer chain because this module gets no signal that file sending
 * is completed.  Re-arranged struct mbx to match struct session so t_upcall
 * could be used.  Made the message list function also a t_upcall.
 * 7/15/91 Changed mind on listing last 5 messages on connect.  Only lists
 * unread messages on connect.  L lists all messages.  Added LL # command
 * at the cost of 146 bytes.  To compile in, use #define LLCMD
 * 7/17/91 - Felt guilty so did some cleanup and squeezed 692 bytes out
 * of her.  Replaced the fancy send parser, mbx_to(), which saved 608
 * of those bytes.  Most of rest was saved by reusing tmpbuf[] where I
 * could.  Another 112 bytes were saved by making message header strings
 * into fixed arrays and using calloc() to init.
 * 8/12/91 - Fixed broken 'g' command caused by using tmpbuf incorrectly.
 * Added clarification to result of kill command.
 * 2/24/92 - added build_line() to cut dupe code (saved 128 bytes) and
 * added some static declarations to make functions private to this module.
 * 3/13/91 - Completed adding the SID2 support for ax25 mbxcall and added
 * ability to read a sub directory off of the public mbox directory.
 * 3/25/91 added a stat() to the readany() function to prevent user from
 * "getting" a directory in Unix
 * 9/5/92 Cleared up some refs to NULL. Note that NULL and NULLCHAR are
 * not the same thing.
 * 10/14/92 Added "w" (what) an alias for the ls command w/o args and added
 * a "jheard" which uses code ifdefed with AX25_HEARD.  Also did some cosmetic
 * work on user responses.  With addition of ax25 T4 timer, this really
 * doesn't need mboxwait any more.  11/25/92
 * 3/3/92 Added "s" arg to mbox to permit sending a butt-in string (k29)
 * 9/93 some changes for OS9/K by N0QBJ and K5JB.  Some error messages
 * made optionally verbose.  Define VERBOSE is you want them; don't if you
 * want to keep the module smaller.
 */

#undef VERBOSE	/* error message in newmailfile() */

#include "options.h"
#include "config.h"
#ifdef AX25
#ifdef AXMBX
#include <string.h>
#include <stdio.h>
#ifdef MSDOS
#include <dos.h>
#endif
#include <time.h>
#include <ctype.h>
#ifdef	UNIX
#include <sys/types.h>
#include <sys/stat.h>
#define SEEK_SET 0
#endif
#include "global.h"
#include "mbuf.h"
#include "ax25.h"
#include "timer.h"
#include "iface.h"
#include "lapb.h"
#ifdef NETROM
#include "netrom.h"
#include "nr4.h"
#endif
#include "ax_mbx.h"
#include "cmdparse.h"
#include "netuser.h"
#include "tcp.h"
/*
#define USERWATCH to watch user's actions
*/

#define LLCMD	/* this adds 146 bytes */

#ifdef CMSG
extern char *cmsg;		/* this, would have to be defined in files.c */
#endif                  /* and path created by fileinit() in main.c */

/* the 3 below, are defined in files.c */
extern char *helpbox;
extern char *public;
extern char *mailspool;

/* miscellaneous declarations */
int queuejob();
int validate_address();
int start_timer();
int stop_timer();
int pax25();
int disc_ax25();
int send_ax25() ;
void rip(),free(),genlog();
#ifdef NETROM
int send_nr4() ;
#endif
extern FILE *dir();
extern void filedir();
extern char versionf[];
#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif
struct mbx *mbox[NUMMBX];
int ax25mbox;

#define BUTTIN	/* to enable "s" */
#ifdef BUTTIN
int butt_flag = FALSE;
#define BUTT_LEN 80
static char butt_msg1[BUTT_LEN];

static char butt_msg2[] =
	"I'm at the console.  If you want to chat, send a \"c\".\015";
#endif

static char mbbanner[] =
	"[NET-$]\015Welcome to the %s TCP/IP Mailbox. I'll see if you have mail...\015";
#ifdef LLCMD
static char mbmenu[] =
	"Read #, Kill #, List, LLast #, Send, Chat, What, Help, Bye >\015";
/*	"(R)ead [#], (K)ill [#], (L)ist, (LL)ast #, (S)end, (C)hat, (H)elp, (B)ye >\015";*/
#else
static char mbmenu[] =
	"Read #, Kill #, List, Send, Chat, What, Help, Bye >\015";
/*	"(R)ead [#], (K)ill [#], (L)ist, (S)end, (C)hat, (H)elp, (B)ye >\015";*/
#endif

static char tmpbuf[MBXLINE];	/* used by various routines */

static void
free_mbx(m)		/* called by mbx_state, mbx_nr4state, and mbx_line when we */
struct mbx *m ;/* come back from chat (ax25 or netrom) */
{
void free();

	genlog(m->name, "outa here");

	if (m->to != NULLCHAR)
		free(m->to) ;
	if (m->tofrom != NULLCHAR)
		free(m->tofrom) ;
	if (m->tomsgid != NULLCHAR)
		free(m->tomsgid) ;
	if(m->name != NULLCHAR){
		free(m->name);
		m->name = NULLCHAR;
	}
	if(m->upload != NULLFILE){
#if defined(_OSK) && !defined(_UCC)
		tmpclose(m->upload);
#else
#ifdef UNIX
		if(pclose(m->upload) < 0)	/* only needed in case of a directory read */
#endif
		fclose(m->upload);
#endif
		m->upload = NULLFILE;
		free(m->ufile);
		m->ufile = NULLCHAR;

	}
	if (m->tfile != NULLFILE)
#if defined(_OSK) && !defined(_UCC)
		tmpclose(m->tfile);
#else
		fclose(m->tfile) ;
#endif

	if(m->mboxwait.state != TIMER_STOP)
		stop_timer(&m->mboxwait);
	if(m->prompter.state != TIMER_STOP)
		stop_timer(&m->prompter);

/*	m->state = MBX_FREE;	think about this */

	mbox[m->mbnum] = NULLMBX ;

	free(m) ;
#ifdef BUTTIN	/* and finally ... */
	butt_flag = FALSE;
	*butt_msg1 = '\0';
#endif
}

static int
mbx_msg(m,msg)
struct mbx *m ;
char msg[] ;
{
	int len ;
	struct mbuf *bp ;
	struct ax25_cb *axp ;
#ifdef NETROM
	struct nr4cb *cb ;
#endif

	len = strlen(msg);
	switch (m->type) {
		case MBX_AX25:
			axp = m->cb.ax25_cb ;
			if ((bp = alloc_mbuf(len+1)) == NULLBUF) {
				disc_ax25(axp) ;
				return(-1);
			}
			bp->cnt = len + 1 ;
			*bp->data = PID_FIRST | PID_LAST | PID_NO_L3 ;
			memcpy(bp->data+1, msg, len) ;
			send_ax25(axp,bp) ;
			break ;
#ifdef NETROM
		case MBX_NETROM:
			cb = m->cb.nr4_cb ;
			if ((bp = alloc_mbuf(len)) == NULLBUF) {
				disc_nr4(cb) ;
				return(-1);
			}
			bp->cnt = len ;
			memcpy(bp->data, msg, len) ;
			send_nr4(cb, bp) ;
			break ;
#endif
	}
	return(0);
}

static void
domboxdisplay(m,remote)
struct mbx *m;
int remote;
{
	int i ;
	struct mbx *lm ;
	extern char tmpbuf[];
	static char *states[] = {"NONE","CMD","SUBJ","DATA","SEND","DISC"} ;
#ifdef NETROM
	static char *mbtype[] = {"NONE","AX25 ","NET/ROM"} ;
#else
	static char *mbtype[] = {"NONE","AX25 "} ;
#endif

	sprintf(tmpbuf," User       State    Type    &cb     ms left\015") ;
	if(remote)
		mbx_msg(m,tmpbuf);
		else{
			rip(tmpbuf);
			printf("%s\n",tmpbuf);
		}
	for (i = 0 ; i < NUMMBX ; i++){
		if ((lm = mbox[i]) != NULLMBX){
#ifdef NETROM
			sprintf(tmpbuf," %-10s %-4s     %-7s %-7x %ld\015",
			 lm->name, states[lm->state], mbtype[lm->type],
			 lm->type == MBX_AX25 ? (char *)lm->cb.ax25_cb : (char *)lm->cb.nr4_cb,
			 lm->mboxwait.count * MSPTICK);
#else
			sprintf(tmpbuf," %-10s %-4s     %-7s %-7x %ld\015",
			 lm->name, states[lm->state], mbtype[lm->type],
			 (char *)lm->cb.ax25_cb, lm->mboxwait.count * MSPTICK);
#endif
			if(remote)
				mbx_msg(m,tmpbuf);
			else{
				rip(tmpbuf);
				printf("%s\n",tmpbuf);
			}
		}
	}
}

int
dombox(argc, argv)
int argc ;
char *argv[] ;
{

	if (argc < 2) {
		domboxdisplay(NULLMBX,0);	/* the 0 indicates local, 1 is remote */
		return(0);
	}

	if (argv[1][0] == 'y' || (strcmp(argv[1],"on") == 0))
		ax25mbox = 1 ;
	else if (argv[1][0] == 'n' || (strcmp(argv[1],"off") == 0))
		ax25mbox = 0 ;
#ifdef BUTTIN
	else if (*argv[1] == 's'){
		int i,j=0;
		for (i = 0 ; i < NUMMBX ; i++)
			if (mbox[i] != NULLMBX)
				break;
		if(i == NUMMBX){
			printf("No one on mbox\n");
			return(0);
		}
		butt_flag = TRUE;	/* prompt routine will send our message */
		if(argc > 2){	/* we have something to say */
			for(i=2;i<argc;i++){
				if(j)
					butt_msg1[j++] = ' ';
				do{
					butt_msg1[j++] = *argv[i]++;
					if(j > BUTT_LEN - 3)
						break;
				}while(*argv[i]);
				if(j > BUTT_LEN - 4)
					break;
			}
			butt_msg1[j++] = '\015';
			butt_msg1[j] = '\0';
		}
	}
#endif	/* BUTTIN */
	else if (argv[1][0] == '?')
		printf("ax25 mailbox is %s\n", ax25mbox ? "on" : "off") ;
	else
#ifdef BUTTIN
		printf("usage: mbox [ y|n|?| s [<message>] ]\n") ;
#else
		printf("usage: mbox [y|n|?]\n") ;
#endif
	return(0);
}

static void
errorexit(m,msg)
struct mbx *m;
char *msg;
{
int doexit();
extern int exitval;

	exitval = 1;
	genlog(m->name,msg);
	doexit();
}

static struct mhdr *
inithdr(m)
struct mbx *m;
{
struct mhdr *mhdr;
struct mhdr *newhdr;
int i;

	/* calloc used to fill struct with zeros */
	if((newhdr = (struct mhdr *)calloc(1,sizeof(struct mhdr))) == NULLMSGHDR)
		errorexit(m,"inithdr: No memory!");	/* no point in hanging around */
	if(m->msgs == 0){
		mhdr = m->firsthdr = newhdr;
		mhdr->next = NULLMSGHDR;
		mhdr->prev = NULLMSGHDR;
	}else{
		for(i=m->msgs,mhdr = m->firsthdr;i > 1;i--)
			mhdr = mhdr->next;
		mhdr->next = newhdr;
		newhdr->prev = mhdr;
		mhdr = newhdr;
	}
	m->msgs += 1;
	mhdr->msgnr = m->msgs;
	return(mhdr);
}


static struct mbx *
newmbx()
{
	int i ;
	struct mbx *m ;

	for (i = 0 ; i < NUMMBX ; i++)
		if (mbox[i] == NULLMBX) {
			if ((m = mbox[i] = (struct mbx *)calloc(1,sizeof(struct mbx)))
				== NULLMBX)
				return(NULLMBX);
			m->mbnum = i ;
			m->msgs = 0;
			m->state = MBX_CMD ;	/* start in command state */
			m->prompter.state = TIMER_STOP;
			m->promptwait = 0;
			return(m);
		}

	/* If we get here, there are no free mailbox sessions */

	return(NULLMBX);
}

static int
g_args(m)	/* return offset to first letter of arg */
struct mbx *m;	/* or nul if none detected after command */
{
int i = 0;

	do
		i++;
		while((m->line[i] != ' ') && (m->line[i] != '\0'));

	if(m->line[i] != '\0'){
		do
			i++;
			while((m->line[i] == ' ') && (m->line[i] != '\0'));

		if(m->line[i] != '\0')
			return(i);
	}
	return(0);
}

int mbx_line();

static void
build_line(m,bp)
struct mbx *m;
struct mbuf *bp;
{
	char c ;
	while (pullup(&bp,&c,1) == 1) {
		if(c == '\012')	/* skip linefeeds */
			continue;
		if (c == '\015') {
			*m->lp = '\0' ;			/* null terminate */
			if (mbx_line(m) == -1) {	/* call the line processor */
				free_p(bp) ;		/* toss the rest */
				break ;				/* get out - we're obsolete */
			}
			m->lp = m->line ;		/* reset the pointer */
		}
		else if ((m->lp - m->line) == (MBXLINE - 1)) {
			*m->lp++ = c ;
			*m->lp = '\0' ;
			if (mbx_line(m) == -1) {
				free_p(bp) ;
				break ;
			}
			m->lp = m->line ;
		}
		else
			*m->lp++ = c ;
	}
}

/* receive upcall for ax.25 */
/* mbx_rx collects lines, and calls mbx_line when they are complete. */
/* If the lines get too long, it arbitrarily breaks them. */

void
mbx_rx(axp,cnt)
struct ax25_cb *axp ;
int16 cnt ;
{
	struct mbuf *bp, *recv_ax25() ;
	struct mbx *m ;

	m = (struct mbx *)axp->user ;
	start_timer(&m->mboxwait);	/* reset idle timer */
	if ((bp = recv_ax25(axp,cnt)) == NULLBUF)
		return ;
	build_line(m,bp);
}

static void
cleanup(m)
struct mbx *m;
{
struct mhdr *mhdr;
struct mhdr *tmphdr;
int i;

	for(i=0,mhdr=m->firsthdr;i<m->msgs;i++){
		tmphdr = mhdr->next;
		free(mhdr);
		mhdr = tmphdr;
	}
	m->msgs = 0;
}

/* state upcall for ax.25 */

void
mbx_state(axp,old,new)
struct ax25_cb *axp ;
int old, new ;
{
	struct mbx *m ;
	void free_mbx();
#ifdef SID2
	void mbx_incom() ;
#endif

	if (new == DISCONNECTED) {
		m = (struct mbx *)axp->user ;
		if(m->msgs)
			cleanup(m);	/* leave mail file alone if we get here */
		axp->user = NULLCHAR ;
		free_mbx(m) ;
	}
#ifdef SID2
	/* this is a hell of a thing to do in a state upcall...
	 * Will come here because it earlier recognized connect to mbxcall
	 * and s_upcall switched to this function - K5JB
	 */
	if(old == DISCONNECTED && new == CONNECTED)
		mbx_incom(axp,0);	/* take another shot at it */
#endif
}

static void
mailfetch(m)
struct mbx *m;
{
FILE *mailfile;
char tmpline[40];
int i;
struct mhdr *mhdr;
extern char tmpbuf[];
char *txtname; /* may be smaller code to do txtname[40] */

	if((txtname = (char *)malloc(sizeof(char) * (strlen(mailspool) +
		strlen(m->name) + 6))) == 0)
		return;
	sprintf(txtname,"%s/%s.txt",mailspool,m->name);
	if((mailfile = fopen(txtname,"r")) == NULLFILE){
		free(txtname);
		return;
	}
	while(fgets(tmpbuf,MBXLINE,mailfile) != NULLCHAR){
		rip(tmpbuf);
/*
From k5jb@k5jbgucci Mon Jun 17 11:32:55 1991
*/
		if(strncmp(tmpbuf,"From ",5) == 0){
			mhdr = inithdr(m);
#ifdef UNIX
			mhdr->fileptr = ftell(mailfile) - strlen(tmpbuf) - 1; /* the EOL */
#else
			mhdr->fileptr = ftell(mailfile) - strlen(tmpbuf) - 2; /* the EOL */
#endif

/* inithdr already incremented msgs */
			if(m->msgs > 1)	/* size is int & fileptr long, but that's OK */
				mhdr->prev->size = (int)(mhdr->fileptr - mhdr->prev->fileptr);
			continue;
		}
/*
Date: Mon, 17 Jun 91 11:32:04 CDT
*/
		if(strncmp(tmpbuf,"Date:",5) == 0){
			if(m->msgs == 0)
				break; /* badly formed file */
			for(i=11;i<26;i++)
				tmpline[i-11] = tmpbuf[i];
			tmpline[i-11] = '\0';	/* inithdr malloced 16 chars */
			strcpy(mhdr->date,tmpline);
			continue;
		}
/*
From: k5jb@k5jbgucci (K5JB_Gucci)
*/
		if(strncmp(tmpbuf,"From:",5) == 0){
			if(m->msgs == 0)
				break; /* badly formed file */
			for(i=6;i<28 && tmpbuf[i] > ' ';i++)
				tmpline[i-6] = tmpbuf[i];
			tmpline[i-6] = '\0';	/* inithdr malloced 23 chars */
			strcpy(mhdr->from,tmpline);
			continue;
		}
/*
Subject: First Message
*/
		if(strncmp(tmpbuf,"Subje",5) == 0){
			if(m->msgs == 0)
				break; /* badly formed file */
			for(i=9;i<39 && tmpbuf[i] >= ' ';i++)
				tmpline[i-9] = tmpbuf[i];
			tmpline[i-9] = '\0';	/* inithdr malloced 31 chars */
			strcpy(mhdr->subj,tmpline);
			continue;
		}
/*
Status: R
*/
		if(strncmp(tmpbuf,"Status",6) == 0){
			if(m->msgs == 0)
				break; /* badly formed file */
			mhdr->status |= STATREAD;
			continue;
		}
	}
	mhdr->size = (int)ftell(mailfile) - ((m->msgs == 1) ? 0 : (int) mhdr->fileptr);

	fclose(mailfile);
	free(txtname);
	return;
}

static void		/* thought this was clever of me.  Since we don't know when */
do_prompt(m)	/* t_upcall is finished, we put our prompt on timer chain */
struct mbx *m; /* if promptwait flag != 0.  Otherwise, send prompt now. */
{
	if(m->promptwait != 0){
		if((m->prompter.state == TIMER_EXPIRE || m->prompter.state == TIMER_STOP)
			&& m->state == MBX_SEND && m->upload != NULLFILE){
							/* reset the timer for another promptwait in secs. */
				m->prompter.start = m->promptwait * 1000L / MSPTICK;
				m->prompter.func = (void(*)())do_prompt;
				m->prompter.arg = (char *)(m);
				start_timer(&m->prompter);
				return;
		}

		if(m->prompter.state != TIMER_STOP)
			stop_timer(&m->prompter);
		if(m->state == MBX_SEND && m->upload == NULLFILE){
			m->state = MBX_CMD;	/* here is where we get the state change */
										/* after a file download - state change on */
			m->promptwait = 0;	/* abort happens in mbx_line() */
		}
	}
#ifdef BUTTIN
	if(butt_flag){
		butt_flag = FALSE;
		if(*butt_msg1 != '\0'){
			mbx_msg(m, butt_msg1);
			*butt_msg1 = '\0';
		}
		mbx_msg(m,butt_msg2);
	}
#endif
	mbx_msg(m, (m->sid & MBX_SID) ? ">\015" : mbmenu);
}

/* modified from doupload from session.c - triggers the t_upcall mechanism
 * which will queue outgoing lines, and close file and free filename when
 * EOF is reached.  In case of Unix pipe, the close will have error but
 * it shouldn't hurt anything
 */

static void
read_fd(m,fd)	/* hand this an open file descriptor - it will launch */
struct mbx *m; /* yer file with t_upcall, followed by a prompt from */
FILE *fd;		/* timer daemon.  Abort sensed in mbx_line() */
{
struct ax25_cb *axp;
#ifdef NETROM
struct nr4cb *cb ;
#endif

	switch(m->type){
		case MBX_AX25:
			axp = m->cb.ax25_cb;
			break;
#ifdef NETROM
		case MBX_NETROM:
			cb = m->cb.nr4_cb ;
			break ;
#endif
	}
	m->upload = fd;
	m->ufile = (char *)malloc(5);	/* something for the upcall to free */
	strcpy(m->ufile,"NADA");
	m->state = MBX_SEND;
	m->promptwait = 5;	/* indicates 5 sec delays */
	do_prompt(m);	/* start the prompt daemon */

/* All set, kick transmit upcall to get things rolling */
	switch(m->type){
		case MBX_AX25:
			(*axp->t_upcall)(axp,axp->paclen * axp->maxframe);
			break;
#ifdef NETROM
		case MBX_NETROM:
			(*cb->t_upcall)(cb, NR4MAXINFO) ;
			break ;
#endif
	}
}

static int				/* this queues things to a temporary file.  Return */
showlist(m,full)		/* is not currently tested.  Full is zero on connect */
struct mbx *m; 		/* "full" can contain number of messages we want to */
int full;				/* list. */
{
int i,new = 0,rtn = 0;
extern char tmpbuf[];
struct mhdr *mhdr;
extern FILE *tmpfile();

#ifdef LLCMD
	if(full < 0 || full > m->msgs)
		full = m->msgs;	/* catch the clowns */
#endif
	if(!m->msgs){
		rtn = mbx_msg(m,"You have no mail.\015");
		do_prompt(m);	/* will fall through to return after else */
	}else{
		mhdr = m->firsthdr;
		for(i=0;i<m->msgs;i++){		/* find out how many were unread */
			if(!mhdr->status & STATREAD)
				new++;
			mhdr = mhdr->next;
		}
		sprintf(tmpbuf,"You have %d message%s  %d unread.",
			m->msgs,m->msgs > 1 ? "s." : ".", new);
		if(!full && new){
			strcat(tmpbuf,"  Unread one");
			strcat(tmpbuf,new == 1 ? " is:" : "s are:");
		}
		strcat(tmpbuf,"\015");
		mbx_msg(m,tmpbuf);
		if(!full && !new){
			do_prompt(m);
			return(0);
		}

/* prepare a temp file.  read_fd will do m->upload = m->upload */
		if ((m->upload = tmpfile()) == NULLFILE)
			return(-1);	/* No test for this currently */
		mhdr = m->firsthdr;
		new = 0;	/* reuse for first message to show */
#ifdef LLCMD
		if(full && (full != m->msgs)){ /* user used ll # command */
			fprintf(m->upload,"The last %d %s:\015",full,full > 1 ? "are" : "is");
			for(i=m->msgs;i>full;i--,new++)
				mhdr = mhdr->next;
		}
#endif
		for(i=new;i<m->msgs;i++,mhdr = mhdr->next)
			if(!(mhdr->status & STATREAD) || full)
				fprintf(m->upload,"%s%s%3d %-22s %s %4d %s\015",
				mhdr->status & STATREAD ? "R" : " ",
				mhdr->status & STATKILL ? "K" : " ",mhdr->msgnr,
					mhdr->from,mhdr->date,mhdr->size,mhdr->subj);
		rewind(m->upload);
		read_fd(m,m->upload);	/* will launch a prompt when done */
	}
	return(rtn);	/* probably will change to no return value */
}

/* Shut down the mailbox communications connection */

static void
mbx_disc(m)
struct mbx *m ;
{
	if(m->mboxwait.state == TIMER_EXPIRE)
		genlog(m->name,"mbox timeout");
	else{
		stop_timer(&m->mboxwait);
		genlog(m->name,"mbox disconnect");
	}
	switch (m->type) {
	  case MBX_AX25:
		disc_ax25(m->cb.ax25_cb) ;
		break ;
#ifdef NETROM
	  case MBX_NETROM:
	  	disc_nr4(m->cb.nr4_cb) ;
		break ;
#endif
	}
}

/* If f_name is nul, checks for mail belonging to current mbox user
 * and sends any mail found.  If f_name is supplied, will download
 * specified file, get command prefixes with public directory -
 * make sure there are no binaries or monsters in there!
 * Returns TRUE if successful, FALSE if not.
 */

static int
readany(m,f_name)
struct mbx *m;
char *f_name;
{
FILE *anyfile;
extern char tmpbuf[];
#ifdef UNIX
struct stat statbuf;
#endif

#ifdef USERWATCH
	printf("Current mbox user: %s is reading a file.\n",m->name);
	fflush(stdout);
#endif
	if(f_name[0] == '\0')
		sprintf(tmpbuf,"%s/%s.txt",mailspool,m->name);
	else
		sprintf(tmpbuf,"%s",f_name);

#ifdef UNIX	/* prevent user from "getting" directory */
	/* an alternative is to fopen with file mode "r+" but files have to
	 * be rw to be downloaded.  MS-DOS won't fopen a dir name. */
	if(stat(tmpbuf,&statbuf) == -1 || statbuf.st_mode & S_IFDIR)
		return FALSE;
#endif

	if ((anyfile = fopen(tmpbuf,"r")) != NULLFILE){
		read_fd(m,anyfile);
		return TRUE;
	}else
		return FALSE;
}

/* Incoming mailbox session via ax.25 */

void
mbx_incom(axp,cnt)
register struct ax25_cb *axp ;
int16 cnt ;
{
	struct mbx *m;
	struct mbuf *bp, *recv_ax25() ;
	char *cp ;
	extern char hostname[] ;
	void ax_tx();
#ifdef NETROM
	void nr4_tx();
#endif
#ifdef SID2
	extern struct ax25_addr bbscall;
	int addreq();
#endif

	if ((m = newmbx()) == NULLMBX) { /* m->msgs will be 0 */
		disc_ax25(axp) ;	/* no memory! disc_ax25() will call lapbstate() */
					/* which has axp->s_upcall still = NULLVFP ?? */
		return ;
	}

	m->type = MBX_AX25 ;	/* this is an ax.25 mailbox session */
	m->cb.ax25_cb = axp ;       /* free() used on name on exit */
	if((m->name = (char *)malloc(sizeof(char) * 10)) != NULLCHAR)
		pax25(m->name,&axp->addr.dest) ;
	cp = strchr(m->name,'-') ;
	if (cp != NULLCHAR)			/* get rid of SSID */
		*cp = '\0' ;
/* Think about this a bit - it is a place holder in the struct for now
	m->parse = NULLVFP; 	see ax_parse()
*/
#ifdef UNIX
	for(cp = m->name; *cp != '\0'; cp++)
		*cp = (char)tolower(*cp);
#endif
	m->lp = m->line ;		/* point line pointer at buffer */
	axp->r_upcall = mbx_rx ;
	axp->s_upcall = mbx_state ;
	axp->t_upcall = ax_tx;	/* used for file uploading - in ax25cmd.c */
	axp->user = (char *)m ;

	/* add an idle timer to the mbox */

	m->mboxwait.start = 600050L / MSPTICK;	/* 10 minutes */
	m->mboxwait.func = (void(*)())mbx_disc;
	m->mboxwait.arg = (char *)m;
	start_timer(&m->mboxwait);

	/* The following is necessary because we didn't know we had a
	 * "real" ax25 connection until a data packet came in.  We
	 * can't be spitting banners out at every station who connects,
	 * since they might be a net/rom or IP station.  Sorry.
	 *
	 * If ax25 mbxcall is defined, we can respond right away to one
	 * who connects to us using that call - K5JB
	 */

#ifdef SID2	/* if call came with mboxcall, we don't gobble packet */
	/* but if it came with our regular call, we do */
	if(!addreq(&axp->addr.source,&bbscall)){
		bp = recv_ax25(axp,cnt) ;		/* get the initial input */
		free_p(bp) ;					/* and throw it away to avoid confusion */
	}
#else
	bp = recv_ax25(axp,cnt) ;
	free_p(bp) ;
#endif
	/* Now say hi */

	if ((bp = alloc_mbuf(strlen(hostname) + strlen(mbbanner) + 2)) == NULLBUF) {
		disc_ax25(axp) ; /* mbx_state will fix stuff up. disc_ax25() calls */
				/* lapbstate with arg DISCPENDING, which will call mbx_state() */
		return ;
	}

	*bp->data = PID_FIRST | PID_LAST | PID_NO_L3 ;	/* pid */
	(void)sprintf(bp->data+1,mbbanner,hostname) ;
	bp->cnt = strlen(bp->data+1) + 1 ;
	send_ax25(axp,bp) ;					/* send greeting message and menu */
	genlog(m->name,"mbox (ax25) connect");

#ifdef CMSG
	if(readany(m,cmsg) == FALSE)
/*		mbx_msg(m,"No connect message\015");*/
#endif
	mailfetch(m);
	showlist(m,0);	/* the 0 is for a partial list - can return an error */
}

/* Incoming mailbox session via net/rom */
#ifdef NETROM
void
mbx_nr4incom(cb)
register struct nr4cb *cb ;
{
	struct mbx *m;
	struct mbuf *bp ;
	char *cp ;
	extern char hostname[] ;
	void mbx_nr4rx(), mbx_nr4state() ;

	if ((m = newmbx()) == NULLMBX) {
		disc_nr4(cb) ;	/* no memory! */
		return ;
	}

	m->type = MBX_NETROM ;	/* mailbox session type is net/rom */
	m->cb.nr4_cb = cb ;

	if((m->name = (char *)malloc(sizeof(char) * 10)) != NULLCHAR)
		pax25(m->name,&cb->user) ;
	cp = strchr(m->name,'-') ;
	if (cp != NULLCHAR)			/* get rid of SSID */
		*cp = '\0' ;
/* Think about this a bit - it is a place holder in the struct for now
	m->parse = NULLVFP; 	see ax_parse()
*/
#ifdef UNIX
	for(cp = m->name; *cp != '\0'; cp++)
		*cp = (char)tolower(*cp);
#endif
	m->lp = m->line ;		/* point line pointer at buffer */
	cb->r_upcall = mbx_nr4rx ;
	cb->s_upcall = mbx_nr4state ;
	cb->t_upcall = nr4_tx;	/* in nr4cmd.c */
	cb->puser = (char *)m ;

	/* add an idle timer to the mailbox */

	m->mboxwait.start = 600050L / MSPTICK;	/* 10 minutes */
	m->mboxwait.func = (void(*)())mbx_disc;
	m->mboxwait.arg = (char *)m;
	start_timer(&m->mboxwait);

	/* Say hi */

	if ((bp = alloc_mbuf(strlen(hostname) + strlen(mbbanner) + 1)) == NULLBUF) {
		disc_nr4(cb) ; /* mbx_nr4state will fix stuff up */
		return ;
	}

	(void)sprintf(bp->data,mbbanner,hostname) ;
	bp->cnt = strlen(bp->data) ;
	send_nr4(cb,bp) ;					/* send greeting message and menu */
	genlog(m->name,"mbox (nr) connect");

#ifdef CMSG
	if(readany(m,cmsg) == FALSE)
/*		mbx_msg(m,"No connect message\015"); keep it quiet */
#endif
	mailfetch(m);
	showlist(m,0);	/* The 0 is for a partial list - can return an error */
}

/* receive upcall for net/rom */
/* mbx_nr4rx collects lines, and calls mbx_line when they are complete. */
/* If the lines get too long, it arbitrarily breaks them. */

void
mbx_nr4rx(cb,cnt)
struct nr4cb *cb ;
int16 cnt ;
{
	struct mbuf *bp ;
	struct mbx *m;

	m = (struct mbx *)cb->puser ;
	start_timer(&m->mboxwait);	/* reset idle timer */
	if ((bp = recv_nr4(cb,cnt)) == NULLBUF)
		return ;
	build_line(m,bp);
}

/* state upcall for net/rom */

void
mbx_nr4state(cb,old,new)
struct nr4cb *cb ;
int old, new ;
{
	struct mbx *m ;
	void free_mbx() ;

	m = (struct mbx *)cb->puser ;

	if (new == NR4STDISC) {
		if(m->msgs)
			cleanup(m);	/* leave mail file alone if we get here */
		cb->puser = NULLCHAR ;
		free_mbx(m) ;
	}
}
#endif	/* NETROM */

static void
newmailfile(m)
struct mbx *m;
{
FILE *mailfile, *bakfile;
extern char tmpbuf[];
char *outfile,*infile;
struct mhdr *mhdr;
int save = 0;
char *errormsg = "newmailfile: No memory";
#ifndef VERBOSE
char *derrormsg = "newmailfile: Disk error";
#endif
int i;

	for(i=0,mhdr = m->firsthdr;i<m->msgs;i++,mhdr = mhdr->next){
		if(mhdr->status & (STATKILL | STATNREAD)){
			save = 1;
			break;
		}
	}
	if(save){
		save = 0; /* we'll reuse this to detect no messages to save */
/* these errorexits will be replaced with some kind of recovery later */
/* on second thought, why salvage a bad situation? */
		if((outfile = (char *)malloc(sizeof(char) * (strlen(mailspool) +
			strlen(m->name) + 6))) == 0)
			errorexit(m,errormsg);
		sprintf(outfile,"%s/%s.txt",mailspool,m->name);

		if((infile = (char *)malloc(sizeof(char) * (strlen(mailspool) +
			strlen(m->name) + 6))) == 0){
			errorexit(m,errormsg);
		}
		sprintf(infile,"%s/%s.bak",mailspool,m->name);

/* we'll rename name.txt to name.bak and take input from name.bak */

		unlink(infile);	/* don't care about error */

		if(rename(outfile,infile) != 0)	/* Unix equiv is in that file set */
#ifdef VERBOSE
   			errorexit(m,"mail file rename failed");
#else
   			errorexit(m,derrormsg);
#endif
		if((bakfile = fopen(infile,"r")) == NULLFILE)
#ifdef VERBOSE
			errorexit(m,"backup mail file open for read failed");
#else
  			errorexit(m,derrormsg);
#endif
		if((mailfile = fopen(outfile,"w")) == NULLFILE)
#ifdef VERBOSE
			errorexit(m,"new mail file open for write failed");
#else
  			errorexit(m,derrormsg);
#endif
		for(i=0,mhdr = m->firsthdr;i<m->msgs;i++,mhdr = mhdr->next){
			if(mhdr->status & STATKILL)
				continue;
			if(fseek(bakfile,mhdr->fileptr,SEEK_SET) != 0)
#ifdef VERBOSE
				errorexit(m,"backup mail file seek failed");
#else
  				errorexit(m,derrormsg);
#endif
/* this is very paranoid */
			if(fgets(tmpbuf,MBXLINE,bakfile) == NULLCHAR)
#ifdef VERBOSE
				errorexit(m,"backup file is empty");
#else
  				errorexit(m,derrormsg);
#endif
	

/* print that first "From " line */
			fprintf(mailfile,"%s",&tmpbuf[0]);
			save = 1;
			while(fgets(tmpbuf,MBXLINE,bakfile) != NULLCHAR){
				if(strncmp(tmpbuf,"From ",5) == 0)
					break;
				fprintf(mailfile,"%s",&tmpbuf[0]);
				if(mhdr->status & STATNREAD && strncmp(tmpbuf,"Subje",5) == 0)
#if defined(UNIX) || defined(_OSK)
					fprintf(mailfile,"Status: R\n");
#else
					fprintf(mailfile,"Status: R\r\n");
#endif
			}
		}
		fclose(mailfile);
		fclose(bakfile);
		if(!save)	/* nothing saved? - most typical case */
			unlink(outfile);
		free(outfile);
		free(infile);
	}	/* if save (the first one) */
	return;
}

/* will copy desired message to name.$$$ - leaves it there to peruse
later.  File descriptor of open file is passed to read_fd for sending */

static FILE *
queuemsg(mhdr,m)
struct mhdr *mhdr;
struct mbx *m;
{
/* read routine */
FILE *txtfile, *tmp_file;
char *txtname,*tmpname;
extern char tmpbuf[];

	if((txtname = (char *)malloc(sizeof(char) * (strlen(mailspool) +
		strlen(m->name) + 6 ))) == 0)
		return(NULLFILE);
	sprintf(txtname,"%s/%s.txt",mailspool,m->name);
	if((txtfile = fopen(txtname,"r")) == NULLFILE){
		free(txtname);
		return(NULLFILE);
	}
	if((tmpname = (char *)malloc(sizeof(char) * (strlen(mailspool) +
		strlen(m->name) + 6))) == 0){
		free(txtname);
		return(NULLFILE);
	}

/* could have used tmpfile() but thought the $$$ file would be interesting */

	sprintf(tmpname,"%s/%s.$$$",mailspool,m->name);
/* In the following w+t puts \r\r\n in the file */
	if((tmp_file = fopen(tmpname,"w+")) == NULLFILE){
		free(txtname);
		free(tmpname);
		return(NULLFILE);
	}

/* Position pointer and get that first "from" line out of the way */
	if(fseek(txtfile,mhdr->fileptr,SEEK_SET) != 0 ||
			fgets(tmpbuf,MBXLINE,txtfile) == NULLCHAR){
		fclose(txtfile);
		free(txtname);
		free(tmpname);
		return(NULLFILE);
	}
	fprintf(tmp_file,"%s",&tmpbuf[0]);
	while(fgets(tmpbuf,MBXLINE,txtfile) != NULLCHAR){
		if(strncmp(tmpbuf,"From ",5) == 0)
			break;
		fprintf(tmp_file,"%s",&tmpbuf[0]);
	}
	fflush(tmp_file);
	rewind(tmp_file);
	fclose(txtfile);
	free(txtname);
	free(tmpname);
	return(tmp_file);
}

/* This opens the data file and writes the mail header into it.
 * Returns 0 if OK, and -1 if not.
 */

static int
mbx_data(m)
struct mbx *m ;
{
	time_t t, time() ;
	char *ptime() ;
	extern char hostname[] ;
	extern FILE *tmpfile();
	extern long get_msgid() ;

	if ((m->tfile = tmpfile()) == NULLFILE)
		return(-1);

	time(&t) ;
	fprintf(m->tfile,"Date: %s",ptime(&t)) ;
	if (m->tomsgid)
		fprintf(m->tfile, "Message-Id: <%s@%s>\n", m->tomsgid, hostname) ;
	else
		fprintf(m->tfile,"Message-Id: <%ld@%s>\n",get_msgid(),hostname) ;
	fprintf(m->tfile,"From: %s%%%s@%s\n",
			m->tofrom ? m->tofrom : m->name, m->name, hostname) ;
	fprintf(m->tfile,"To: %s\n",m->to) ;
	fprintf(m->tfile,"Subject: %s\n",m->line) ;
	if (m->stype != ' ')
		fprintf(m->tfile,"AX.25(mbox)_Msg_Type: %c\n", m->stype) ;
	fprintf(m->tfile,"\n") ;

	return(0);
}

static int	/* replacement send command parser.  Saved 608 bytes - K5JB */
mbx_to(m)	/* "S" already OK, also type (T,B,P) recorded.  Comes here */
struct mbx *m ;	/* with whole line entact.  Returns -1 on error */
{
char *cp1,*cp2;
extern char tmpbuf[];

	cp2 = cp1 = m->line;
	if(*cp1 == '\0' || (cp1 = strchr(cp1,' ')) == NULLCHAR) /* find first space */
		return(-1);                                      /* after "s" */

	while(*cp1 != '\0'&& *cp1 == ' ')			/* any more? */
		cp1++;
								/* now should be at recipient */
	while(*cp1){			/* remove all spaces from rest of line */
		if(*cp1 != ' ')	/* and shuffle back into line */
			*cp2++ = *cp1++;
			else
				cp1++;
	}
	*cp2 = *cp1;
	cp1 = m->line;
	if(!*cp1)
		return(-1);
	cp2 = tmpbuf;

	while(*cp1){	/* get to, upto and include '@' */
		*cp2++ = *cp1++;
		if(*cp1 == '@' || *cp1 == '<' || *cp1 == '$'){
			break;
		}
	}
	if(tmpbuf[0] == '@')
		return(-1); 	/* only had "@..." */
	switch(*cp1){
		case '<':
		case '$':
			*cp2 = '\0';
		break;
		case '@': 				/* see what is after the '@' */
			*cp2++ = '@';
			if(!*(++cp1)) 			/* only had "tocall@" */
				return(-1);
			while(*cp1){	/* upto next delimiter */
				if(*cp1 == '<' || *cp1 == '$')
					break;
				*cp2++ = *cp1++;
			}

	} /* switch */
	*cp2 = '\0';
	if ((m->to = malloc(strlen(tmpbuf) + 1)) == NULLCHAR)
		return(-1);		/* no room for to address */
	strcpy(m->to,tmpbuf);

	if(*cp1 == '<'){
		cp2 = tmpbuf;	/* reset target pointer */
		if(!*(++cp1)) 			/* only had "<" */
			return(-1);
		while(*cp1 && *cp1 != '$')	/* see what is after the '<' */
			*cp2++ = *cp1++;
		*cp2 = '\0';
		if (cp2 == tmpbuf || (m->tofrom = malloc(strlen(tmpbuf) + 1)) == NULLCHAR)
			return(-1);		/* no tofrom or no room for to address */
		strcpy(m->tofrom,tmpbuf);
	}
	if(*cp1 == '$'){
		cp2 = tmpbuf;	/* reset target pointer */
		while(*cp1){	/* see what is after the '$' assume BBS won't jack */
			*cp2++ = *cp1++;           /* around with us */
		}
		*cp2 = '\0';
		if ((m->tomsgid = malloc(strlen(tmpbuf) + 1)) == NULLCHAR)
			return(-1);		/* no room for to address */
		strcpy(m->tomsgid,tmpbuf);
	}
	return(0);

}

static int
mbx_line(m)       /* returns -1 if there is a disc to dump */
struct mbx *m ;	/* receive buffers */
{
int do_cmd(), do_subj(), do_data();

	switch(m->state){

		case MBX_SEND:                /* user sent a character during download */
			if(m->upload != NULLFILE){	/* t_upcall sets NULLFILE when finished */
#if defined(_OSK) && !defined(_UCC)
				tmpclose(m->upload);
#else
#ifdef UNIX
				if(pclose(m->upload) < 0)	/* only needed in case of a directory */
#endif					
						/* read in Unix which uses a pipe */
				fclose(m->upload);
#endif
				m->upload = NULLFILE;	/* do_prompt daemon will clean up timer */
				free(m->ufile);
				mbx_msg(m,"\015Interrupted at your request.\015");
			}
			/* rethink this - seems inconsistent to be outside if() above */
			if(m->prompter.state != TIMER_STOP)
				stop_timer(&m->prompter);
			m->promptwait = 0;
			m->state = MBX_CMD;		/* this state means send was completed */
			do_prompt(m);
			return(0);

		case MBX_CMD:
			return(do_cmd(m));

		case MBX_SUBJ:
			return(do_subj(m));

		case MBX_DATA:
			return(do_data(m));

/* m->state set to MBX_DISC by read_fd if cb->state != 0 -1 is needed to
 * dump queued buffers */

		case MBX_DISC:
			return(-1);

	}
	return(0);	/* shouldn't get here */
}

static int	/* only return -1 on disconnect */
do_cmd(m)
struct mbx *m;
{

void ax_session(), mbx_disc();
#ifdef NETROM
void nr4_session();
#endif
/* Let's try using tmpbuf for this char fullfrom[80] ;*/
extern char tmpbuf[];
#ifdef	SHOWNODES
register struct nrroute_tab *rp ;
char buf[16] ;
char *cp ;
#endif
char tempstr1[40];      /* suspect tmpbuf[] would be safe.  */
int times, j, i = 0;    /* 40 is way much for tempstr1.  It only has filename */
char tempstr2[40];	/* only used for filedir() */
FILE *d_file;
struct mhdr *mhdr;	/* temporary message header pointer */
int atoi();

#ifdef AX25_HEARD
void free();
char *pp;
extern char *homedir;
extern char *buildheard();
#endif

#define WHAT
#ifdef WHAT
	if(m->line[0] == 'w' || m->line[0] == 'W'){	/* make a "what" command */
		m->line[0] = 'l';
		m->line[1] = 's';
		m->line[2] = '\0';
	}
#endif

	switch (tolower(m->line[0])) {
		case 'b':	/* bye - bye */
			if(m->msgs){
				newmailfile(m);
				cleanup(m);
			}
			mbx_disc(m) ;
			return(-1);	/* tell line processor to quit */
		case 'c':	/* chat */
			mbx_msg(m,"Messages not saved. If I don't respond, Disconnect, reconnect and send mail.\015");
			stop_timer(&m->mboxwait);
			if(m->msgs){
				newmailfile(m);
				cleanup(m);
			}
			genlog(m->name,"went to chat");

			switch (m->type) {
				case MBX_AX25:
					m->cb.ax25_cb->user = NULLCHAR ;
					ax_session(m->cb.ax25_cb,0) ;	/* make it a chat session */
					break ;		/* this hands cb->state off to ax_state which */
									/* is benign with respect to struct mbx */
#ifdef NETROM
				case MBX_NETROM:
					m->cb.nr4_cb->puser = NULLCHAR ;
					nr4_session(m->cb.nr4_cb) ;
					break ;
#endif
			}
			free_mbx(m) ;
			return(-1);
		case 'm':
			sprintf(tmpbuf,"I have mail for: ");
			times = 0;
			sprintf(tempstr2,"%s/*.txt",mailspool);	/* drat! Hate this! */
			for(;;){
				filedir(tempstr2,times++,tempstr1);
				if(tempstr1[0] == '\0')
					break;
				for(j=0;j<8;j++)
					if(tempstr1[j] == '.'){
						tempstr1[j] = '\0';
						break;
					}
				sprintf(&tmpbuf[strlen(tmpbuf)],"%s ",tempstr1);
				if(strlen(tmpbuf) > 70){
					strcat(tmpbuf,"\015");
					mbx_msg(m,tmpbuf);
					tmpbuf[0] = '\0';
				}
			}
			if(times == 1)
				strcat(tmpbuf,"Nobody!");
			strcat(tmpbuf,"\015");
			mbx_msg(m,tmpbuf);
			break;

		case 'l':	/* l[l n] (mail) or ls (dir) command, with optional */
			i = 0;
			switch(tolower(m->line[1])){    /* arguments */
#ifdef LLCMD
				case 'l':            /* "ll" */
					i = g_args(m);	/* this will require a space before the number */
					i = atoi(&m->line[i]);	/* note fall through */
#endif
				default:                /* no second character */
					showlist(m,i == 0 ? m->msgs : i);	/* show a full list */
					return(0);
				case 's':		/* ls command */
					i = g_args(m);
					if(m->line[i] == '-' && m->line[i+1] != '\0') /* trapdoor */
						d_file = dir(&m->line[i+1],1);
					else{
					/* in the following, we will hope that not more than
					 * one user at a time uses tmpbuf
					 */
#ifdef UNIX

						if(i){
							/* Just in case we had a long line from a loose upload */
							m->line[MBXLINE - strlen(public) - 2] = '\0';
							sprintf(tmpbuf,"%s/%s",public,&m->line[i]);
						}else
							sprintf(tmpbuf,"%s",public);

						/* first stop the .. trick then remove noxious chars */
						for(i=0;i<strlen(tmpbuf);i++)
							if((tmpbuf[i] == '.' && tmpbuf[i+1] == '.') ||
									tmpbuf[i] == '|' || tmpbuf[i] == ';' ||
									tmpbuf[i] == '>' || tmpbuf[i] == '<'){
								tmpbuf[i] = '\0';
								break;
							}

						d_file = dir(tmpbuf,1);

#else 	/* deal with MSDOS */

#define SUBDIR	/* this costs us 192 bytes */

#ifdef SUBDIR
						if(i){	/* First shorten line if necessary */
							m->line[MBXLINE - strlen(public) - 6] = '\0';
							sprintf(tmpbuf,&m->line[i]);
							for(j=0;j<strlen(tmpbuf);j++)	/* remove any .. */
								if(tmpbuf[j] == '.' && tmpbuf[i+1] == '.'){
									tmpbuf[j] = '\0';
									break;
								}
							sprintf(m->line,"%s/%s",public,tmpbuf);
							if((d_file = dir(m->line,1)) == NULLFILE){
								/* then try for a directory - here is where "6"
								 * came from in the truncation above */
								sprintf(m->line,"%s/%s/*.*",public,tmpbuf);
								d_file = dir(m->line,1);
							}
						}else
#endif
						{	/* for the else above */
							/* no arg, just show public directory */
							sprintf(m->line,"%s/*.*",public);
							d_file = dir(&m->line,1);
						}
#endif
					}	/* else normal ls command */

					if(d_file != NULLFILE){
						read_fd(m,d_file);
						return(0);
					}else
						mbx_msg(m,"Nothing found.\015");
			} /* l command (second character) switch */
			break;

		case 'd':	/* 'download' - alias added k29 */
		case 'g':		/* get a file, needs argument */
			i = g_args(m);
			if(!i)
				mbx_msg(m,"No file specified.\015");
			else{
				if(m->line[i] == '-')          /* a little frivolity here */
					j = readany(m,&m->line[i+1]); /* read any file, anywhere, */
				else{                            /* if you know this */
					sprintf(tmpbuf,"%s/%s",public,&m->line[i]);
					sprintf(m->line,"%s",tmpbuf); /* readany() reuses tmpbuf */
					j = readany(m,m->line);
				}
				if(j)
					return(0);
				mbx_msg(m,"File not found.\015");
			}
			break;

		case 'h':
			if(readany(m,helpbox))
				return(0);
			mbx_msg(m,"No help available\015");
			break;

#ifdef AX25_HEARD
		case 'j':
			pp = buildheard(1);	/* the '1' is a flag that this is the mailbox */
			if(pp == NULLCHAR){
				mbx_msg(m,"No heard list available\015");
				break;
			}
			readany(m,pp);	/* won't worry about errors */
			free(pp);
			return(0);	/* readany() takes care of next prompt */
#endif

		case 'k':
			if(m->msgs == 0)
				break;
			i = g_args(m);
			if(!i && m->msgs > 1)
				mbx_msg(m,"Follow K with a space and number\015");
			else{
				j = ((m->msgs == 1) ? 1 : atoi(&m->line[i]));
				if(j == 0 || j > m->msgs)
					break;
				mhdr = m->firsthdr;
				while(--j)
					mhdr = mhdr->next;
				mhdr->status |= STATKILL;
				sprintf(tmpbuf,"Nr. %d is history after you do \"Bye\"!\015",mhdr->msgnr);
				mbx_msg(m,tmpbuf);
			}
			break;
		case 'r':
			if(m->msgs == 0){
				mbx_msg(m,"No mail waiting\015");
				break;
			}
			i = g_args(m);
			if(!i && m->msgs > 1)
				mbx_msg(m,"Follow R with a space and number\015");
			else{
				j = ((m->msgs == 1) ? 1 : atoi(&m->line[i]));
				if(j == 0 || j > m->msgs)
					break;
				mhdr = m->firsthdr;
				while(--j)
					mhdr = mhdr->next;
				if((d_file = queuemsg(mhdr,m)) == NULLFILE) /* see below */
				/* fatal -- maybe add recovery? */
					errorexit(m,"queuemsg file fail");
				read_fd(m,d_file);  /* this could be added to queuemsg */
/* if new read we will have to add a line to new file */
				if(!mhdr->status & STATREAD)
					mhdr->status |= (STATREAD | STATNREAD);
				return(0);
			}
			break;
#ifdef SHOWNODES           /* this needs work, if I use it at all */
		case 'n':   /* show nodes */
			column = 1 ;
			sprintf(tmpbuf,"The following nodes were heard at %s\015",
				hostname);
			mbx_msg(m,tmpbuf);
			for (i = 0 ; i < NRNUMCHAINS ; i++)
				for (rp = nrroute_tab[i]; rp != NULLNRRTAB;
						rp = rp->next) {
					strcpy(buf,rp->alias) ;	/* remove trailing spaces */
					if ((cp = strchr(buf,' ')) == NULLCHAR)
						cp = &buf[strlen(buf)] ;
					if (cp != buf)  /* don't include colon for null alias */
						*cp++ = ':' ;
					pax25(cp,&rp->call) ;
					if (column == 1)
						sprintf(tmpbuf,"%-16s  ",buf) ;
					else {
						sprintf(tempstr1,"%-16s  ",buf) ;
						strcat(tmpbuf,tempstr1);
					}
					if (column++ == 4) {
						strcat(tmpbuf,"\015");
						mbx_msg(m,tmpbuf);	/* don't do it this way! */
						column = 1 ;
					}
				}

			if (column != 1)
				mbx_msg(m,"\015");
			break;
#endif	/* SHOWNODES */
		case 's': {
			int badsubj = 0 ;

/* Get S-command type (B,P,T, etc.) */

			if (m->line[1] == '\0')
				m->stype = ' ' ;
			else
				m->stype = toupper(m->line[1]) ;

			if (mbx_to(m) == -1) {
				if (m->sid & MBX_SID)
					mbx_msg(m,"NO\015") ;
				else {
					mbx_msg(m,
						"S command syntax error - format is:\015") ;
					mbx_msg(m,
					  "  S name [@ host] [< from_addr] [$bulletin_id]\015") ;
				}
				badsubj++ ;
			}
			else if (validate_address(m->to) == 0)	 {
				if (m->sid & MBX_SID)
					mbx_msg(m, "NO\015") ;
				else
					mbx_msg(m, "Bad user or host name\015") ;
				free(m->to) ;
				m->to = NULLCHAR ;
				if (m->tofrom) {
					free(m->tofrom) ;
					m->tofrom = NULLCHAR ;
				}
				if (m->tomsgid) {
					free(m->tomsgid) ;
					m->tomsgid = NULLCHAR ;
				}
				badsubj++ ;
			}

			if (!badsubj){
				m->state = MBX_SUBJ ;
				mbx_msg(m,	(m->sid & MBX_SID) ? "OK\015" : "Subject:\015") ;
				return(0);
			}
			break;
		}
		case 'u':
			domboxdisplay(m,1);	/* the 0 indicates local, 1 is remote */
			break;
		case 'v':
			sprintf(tmpbuf,"This is version %s\015",versionf);
			mbx_msg(m,tmpbuf);
			break;
		case '[':	/* This is a BBS - say "OK", not "Subject:" */
		  {
			int len = strlen(m->line) ;
			if (m->line[len - 1] == ']') { /* must be an SID */
				m->sid = MBX_SID ;
				/* Now check to see if this is an RLI board. */
				/* As usual, Hank does it a bit differently from */
				/* the rest of the world. */
				if (len >= 5)		/* [RLI] at a minimum */
					if (strncmp(&m->line[1],"RLI",3) == 0)
						m->sid |= MBX_SID_RLI ;

				mbx_msg(m,">\015") ;
			}
		  }
		  return(0);

		case 'f':
			if (m->line[1] == '>' && (m->sid & MBX_SID)) {
				/* RLI BBS' expect us to disconnect if we */
				/* have no mail for them, which of course */
				/* we don't, being rather haughty about our */
				/* protocol superiority. */
				if (m->sid & MBX_SID_RLI) {
					mbx_disc(m) ;
					return(-1);
				} else
					mbx_msg(m,">\015") ;
				return(0);
			}
				/* Otherwise drop through to "huh?" */
		default:
			mbx_msg(m,"Huh?\015") ;
	} /* switch */
	do_prompt(m);	/* all breaks from switch come here.  When file sent, */
	return(0);     /* with read_fd, prompt handled with timer daemon */
}


static int
do_subj(m)
struct mbx *m;
{
	if (mbx_data(m) == -1) {
		mbx_msg(m,"Can't create temp file for mail\015") ;
		do_prompt(m);
		free(m->to) ;
		m->to = NULLCHAR ;
		if (m->tofrom) {
			free(m->tofrom) ;
			m->tofrom = NULLCHAR ;
		}
		if (m->tomsgid) {
			free(m->tomsgid) ;
			m->tomsgid = NULLCHAR ;
		}
		m->state = MBX_CMD ;
		return(0);
	}
	m->state = MBX_DATA ;
	if ((m->sid & MBX_SID) == 0)
		mbx_msg(m,
		  "Enter message.  Finish with /EX or ^Z in first column:\015") ;
	return(0);
}

static int
do_data(m)
struct mbx *m;
{
char *host ;
extern char hostname[] ;
extern char tmpbuf[];

	if (m->line[0] == 0x1a ||
		strcmp(m->line, "/ex") == 0 ||
		strcmp(m->line, "/EX") == 0) {
		if ((host = strchr(m->to,'@')) == NULLCHAR)
			host = hostname ;		/* use our hostname */
		else
			host++ ;				/* use the host part of address */

			/* make up full from name for work file */
		(void)sprintf(tmpbuf,"%s@%s",m->name,hostname) ;

		fseek(m->tfile,0L,0) ;		/* reset to beginning */
		if (queuejob((struct tcb *)0,m->tfile,host,m->to,tmpbuf) != 0)
			mbx_msg(m,
				"Couldn't queue message for delivery\015") ;

		free(m->to) ;
		m->to = NULLCHAR ;
		if (m->tofrom) {
			free(m->tofrom) ;
			m->tofrom = NULLCHAR ;
		}
		if (m->tomsgid) {
			free(m->tomsgid) ;
			m->tomsgid = NULLCHAR ;
		}
#if defined(_OSK) && !defined(_UCC)
		tmpclose(m->tfile);
#else
		fclose(m->tfile) ;
#endif
		m->tfile = NULLFILE ;
		m->state = MBX_CMD ;
		do_prompt(m);
		return(0);
	}
/* not done yet! */
	fprintf(m->tfile,"%s\n",m->line) ;
	return(0);
}
#endif /* AXMBX */
#endif /* AX25 */

