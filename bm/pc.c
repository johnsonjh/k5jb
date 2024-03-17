/* This file contains machine specific functions */
#include <stdio.h>

/* This file is an attempt at keeping thing portble while taking
* advantage of some of the faster io functions in the turbo lib.
* It seems they all do it their own way but here it goes
*/

#include "config.h"

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

#ifdef AZTEC	/* if anybody uses Aztec, check config.h */
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

#ifdef SIGNALS
/* dummy do nothing */
int catchit() {}

void
setsignals()
{
	ctrlbrk(catchit);	/* this doesn't make sense K5JB */
}
#endif

#ifdef VIDEO
void
setvideo(s)
char *s;
{
	if (strncmp("bios",s,4) == 0)
		directvideo = 0;
	else
		directvideo = 1;
}
#endif

