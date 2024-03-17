/* This file contains machine specific functions */
#include <stdio.h>

/* This file is an attempt at keeping thing portble while taking
* advantage of some of the faster io functions in the turbo lib.
* It seems they all do it their own way but here it goes
*/

/* directory search utility for DOS */
#if	defined(MICROSOFT)
#include <sys/types.h>
#endif
#if	defined(MICROSOFT) || defined(AZTEC)
#include <signal.h>
#endif
#if	defined(MICROSOFT) || defined(__TURBOC__)
#include <sys/stat.h>
#include <dos.h>
#include <conio.h>
#define ST_RDONLY	0x01	/* read only file */
#define ST_HIDDEN	0x02	/* hidden file */
#define ST_SYSTEM	0x04	/* system file */
#define ST_VLABEL	0x08	/* volume label */
#define ST_DIRECT	0x10	/* file is a sub-directory */
#define ST_ARCHIV	0x20	/* set when file has been written and closed */
#endif
#ifdef AZTEC
#include <stat.h>
#endif

#define REGFILE	(ST_HIDDEN|ST_SYSTEM|ST_DIRECT)
#define	SET_DTA		0x1a
#define	FIND_FIRST	0x4e
#define	FIND_NEXT	0x4f

struct dirent {
	char rsvd[21];
	char attr;
	short ftime;
	short fdate;
	long fsize;
	char fname[13];
};
/* wildcard filename lookup */
filedir(name,times,ret_str)
char *name;
int times;
char *ret_str;
{
	register char *cp,*cp1;
	static struct dirent sbuf;
#if	defined(MICROSOFT) || defined(__TURBOC__)
	union REGS regs;
#endif

	bdos(SET_DTA,(unsigned)&sbuf,0);	/* Set disk transfer address */

#if	defined(MICROSOFT) || defined(__TURBOC__)
	regs.h.ah = (times == 0) ? FIND_FIRST : FIND_NEXT;
	regs.x.dx = (unsigned int) name;
	regs.x.cx = (unsigned int) REGFILE;
	intdos(&regs,&regs);
	if (regs.x.cflag) 
		sbuf.fname[0] = '\0';
#else
	/* Find matching file */
	if(dos(times == 0 ? FIND_FIRST:FIND_NEXT,0,REGFILE,name,0,0) == -1)
		sbuf.fname[0] = '\0';
#endif

	/* Copy result to output, forcing to lower case */
	for(cp = ret_str,cp1 = sbuf.fname; cp1 < &sbuf.fname[13] && *cp1 != '\0';)
		*cp++ = tolower(*cp1++);
	*cp = '\0';
}

#ifdef DEAD
/* This function should put the tty in a mode such that signgle characters
* can be read without waiting for a complete line. Echo should be on.
*/
setrawmode()
{}

/* This function should restore the tty modes back to cooked mode */
setcookedmode()
{}
#endif

/* This function return one charater form the keyboard. It will wait
* for a character to be input. This function will echo the character.
* This funtion will return afer each character is typed if rawmode is set
*/
#ifdef RAW
int
getrch()
{
	int	c;
#if	defined(AZTEC) || defined(MICROSOFT) || defined(__TURBOC__)
	c = bdos(1,0,0);
#endif
	return(c & 0xff);
}
#endif
#ifdef DEAD
/* This function show clear screen and put cursor at top of screen */
screen_clear()
{
#ifdef AZTEC
	extern	int	scr_clear();
	scr_clear();	/* from lib S */
#endif
#if	defined(MICROSOFT) || defined(__TURBOC__)
	/* clear screen using window scroll up */
	union REGS regs;
	regs.h.ah = 6;
	regs.h.al = 0;
	regs.h.ch = 0;
	regs.h.cl = 0;
	regs.h.dh = 24;
	regs.h.dl = 79;
	regs.h.bh = 7;
	int86(0x10,&regs,&regs);
	/* home the cursor */
	regs.h.ah = 2;
	regs.h.bh = 0;
	regs.h.dh = 0;
	regs.h.dl = 0;
	int86(0x10,&regs,&regs);
#endif
}
#endif /* DEAD */
#ifdef AZTEC
/* This is the aztec specific setvbuf since the Aztec lib doesnt have one */
setvbuf(stream,buffer,type,size)
register FILE *stream;
char *buffer;
int type;
int size;
{
	if (stream->_buff)
		return;
	if (buffer && type != _IONBF) {
		stream->_buff = buffer;
		stream->_buflen = size;
	} else {
		stream->_buff = &stream->_bytbuf;
		stream->_buflen = 1;
	}
}
#endif




#if	defined(MICROSOFT) || defined(AZTEC)
setsignals()
{
	signal(SIGINT,SIG_IGN);
}
#endif

#if	defined(__TURBOC__)
/* dummy do nothing */
int catchit() {}

setsignals()
{
	ctrlbrk(catchit);	/* this doesn't make sense K5JB */
}


setvideo(s)
char *s;
{
	if (strncmp("bios",s,4) == 0)
		directvideo = 0;
	else
		directvideo = 1;
}
#endif

#if	defined(__TURBOC__) && defined(DV)
/* I use my own gets to get around the desqview raw mode bug
   which causes gets not to work right.
   I am doing it only for turbo C right now since its a pain to
   get it right for them all.
   it reads straight from console not via stdin.
   Further mucked with it because it looked bad on the Desqview screen,
   and earlier versions of DV have a bug in gets all right.  K5JB
*/
char *
gets(s)
char *s;
{
	register char *p;
/*	register int c;*/
	register char c;
	p = s;
/*	while ((c = getrch()) != EOF){*/
	while(1){
	   while(!kbhit())
	   ;
	   c = (char)getch();
	   if ( c == '\b' && p > s) {
		p--;
		printf("\b \b");
	   }
		else if ( c == '\n' || c == '\r') {
			break;
		} else {
			*p++ = c;
			printf("%c",c);
		}
	}
	*p = '\0';
/*	putch('\r');*/	/* try adding this */
	printf("\n");
	return(s);
}
#endif
