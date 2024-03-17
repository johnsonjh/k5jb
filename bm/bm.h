/* bm.h -- definitons for bm.c that aren't included elsewhere */

/* types of config file lines that we know how to handle */
#define NONE	0
#define MAXLET	1
#define SMTP	2
#define HOST	3
#define USER	4
#define EDIT	6
#define NAME	7
#define REPLY	8
#define MBOX	9
#define RECORD	10
#define SCREEN	11
#define FOLDER	12
#define MQUEUE	13
#define ALIAS	14
#define TIMEZONE 15	/* Added to Unix - K5JB */

#ifdef UNIX
#define WWRAP 5	/* command added for wordwrap in Unix */
#endif

#define WORK	"*.wrk"		/* work file type */
#ifdef MSDOS
#define	MAXROWS		25	/* number of lines on display */
#define	MAXCOL		80	/* number of lines on display */
#else
#define	MAXROWS		24	/* number of lines on display */
#define	MAXCOL		80	/* number of lines on display */
#endif

/* message status */
#define	DELETE	1
#define	READ	2

#define NLET	300			/* default size of letter array */
#define MAXARGS	16

#define SLINELEN	64
#define LINELEN 128	/* was 256, need to revisit this and cut more - K5JB */

/* a mailbox list entry */
struct let {
	long	start;
	long	size;
	int	status;
};

/* address structure */
struct addr {
	struct addr *next;
	char *user;
	char *host;
	int sent;
};

/* token used for a string and its token */
struct token {
	char *str;
	char type;
};

/* global definitions */
extern char
 	*hostname,		/* name of this host from rc file */
	*username,		/* name of this user from rc file */
	*fullname,		/* name of this user from rc file */
	*replyto,		/* return address fro reply-to */
	*maildir,		/* default mail directory */
	*editor,		/* user's favorite editor program */
	*savebox,		/* user's mbox for the s command */
	*record,		/* place to store a copy of snet mail for you */
	*folder,		/* directory used for save and write commands */
	*mfilename;		/* for the -f option */

extern unsigned maxlet;		/* max messages */
extern int current;		/* the current message number */
extern int nmsgs;		/* number of messages in this mail box */
extern int newmsgs;		/* number of new messages in mail box */
extern int change;		/* mail file changed */
extern FILE *mfile;		/* mail data file pointer */
extern int tty;			/* is standard in a tty ? */
extern int qflag;		/* just queue no headers */
extern int mlock();
extern void rmlock(),displaymsg(),free(),rip(),filedir(),loadconfig(),
	setsignals(),listnotes(),getcommand(),setvideo(),catchit();
extern void dohelp(),msgtofile(),savemsg(),writemsg(),bmexit(),reply(),
	bouncemsg(),listqueue(),killjob(),mboxnames(),reinit(),delmsg();
extern void printnext(),setcookedmode(),screen_clear();
extern int readnotes(),dosmtpsend(),getopt(),lockit(),initnotes(),
	rc_line_type(),closenotes();
extern int getrch(), setrawmode(), isnewmail();
extern struct addr *make_tolist();
extern long get_msgid();
extern char *mqueue;
extern char notename[];
extern char notefile[];
extern char nomail[];
extern char badmsg[];
extern struct let *mbox;
extern char *fgets(),*gets(),*savestr();
#ifndef COH386
extern void *malloc(),*calloc();
#endif
extern void exit(),perror();
extern int parse(),htype();

/* General purpose NULL - Note that Unix, Turbo C agree with this, Coherent
 * defined NULL as (char *)0, this code defined it as (void *)0 - K5JB
 */
#ifdef NULL
#undef NULL
#endif
#define	NULL 0

#define	NULLCHAR (char *)0	/* Null character pointer */
#define	NULLFP	 (int (*)())0	/* Null pointer to function returning int */
#define	NULLVFP	 (void (*)())0	/* Null pointer to function returning void */
#define	NULLFILE (FILE *)0	/* Null file pointer */
#define	NULLADDR (struct addr *)0	/* Null address */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
