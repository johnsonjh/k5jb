/* Added redisplay of previous line with some limited line editing.  Made
 * changes to prevent array over-writes.  Note that echo() and noecho()
 * are only used by ftpcli.c during the password part of a login.  Elim-
 * inating them only reduced code size by 50 or so bytes.  The editing
 * costs 436 bytes and can be eliminated with an #undef EDIT.
 * 3/18/92 - K5JB
 */
/* TTY input driver */
#include <stdio.h>
#include <ctype.h>
#include "config.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define EDIT

int ttymode;
#define	TTY_LIT	0		/* Send next char literally */
#define	TTY_RAW	1
#define TTY_COOKED	2

int ttyecho=1;
#define	TTY_NOECHO	0
#define	TTY_ECHO	1

#ifdef	FLOW
int ttyflow=1;
#endif

#define	LINESIZE	80

#ifdef EDIT
#define CTLE	5	/* redisplay last line */
#define CTLF	6	/* cursor forward one word */
#define CTLD	4  /* cursor forward one char */
#define CTLS	19 /* cursor backward (non-destructive) */
#endif

#define CTLA	1  /* cursor back one word */
#define CTLR	18	/* redisplay current line */
#define	CTLY	25 /* delete current line */
#define CTLU	21 /* ditto - backwards compat. */
#define	CTLV	22
#define	CTLW	23
#define	CTLZ	26
#define	RUBOUT	127

void
raw()
{
	ttymode = TTY_RAW;
#ifdef	ATARI_ST
	set_stdout(ttymode);	/* CR/LF vs LF madness...  -- hyc */
#endif
}

void
cooked()
{
	ttymode = TTY_COOKED;
#ifdef	ATARI_ST
	set_stdout(ttymode);
#endif
}

void
echo()
{
	ttyecho = TTY_ECHO;
}

void
noecho()
{
	ttyecho = TTY_NOECHO;
}

/* Accept characters from the incoming tty buffer and process them
 * (if in cooked mode) or just pass them directly (if in raw mode).
 * Returns the number of characters available for use; if non-zero,
 * also stashes a pointer to the character(s) in the "buf" argument.
 */
 /*Control-R added by df for retype of lines - useful in Telnet */
 /*Then df got impatient and added Control-W for erasing words  */
 /* Control-V for the literal-next function, slightly improved
  * flow control, local echo stuff -- hyc */

/* editing, put this in edit.hlp for online reminder:
Editing for commands
^E redisplay last line
^A backup one word
^S backup one char
^D fwd one char
^F fwd one word
Other editing:
^R redisplay current line buffer
^W erase word backward
Special characters:
^V next char literal
^U or ^Y kill line
^H or DEL destructive backspace
*/

int
ttydriv(c,buf)
char c;
char **buf;
{
	static char linebuf[LINESIZE];
#ifdef EDIT
	static char lastline[LINESIZE];
	static char *ep = linebuf;
	static int editing;
	int i;
#endif
	int erase = FALSE;
	static char *cp = linebuf;
	char *rp ;
	int cnt;

#ifdef EDIT
	if(*lastline == '\0')	/* only happens first time through */
		*lastline = '\015';
#endif

	if(buf == (char **)0)
		return 0;	/* paranoia check */

	cnt = 0;

	switch(ttymode){
		case TTY_LIT:
			ttymode = TTY_COOKED;	/* Reset to cooked mode */
			*cp++ = c;	/* run a slight risk of array violation here */
#ifdef UNIX
			putchar('.');	/* Terminal may prefer no Ctrl-chars */
#else
			putchar(c);	/* I know it isn't noxious on MS-DOS */
#endif
			break;
		case TTY_RAW:
			*cp++ = c;
			cnt = cp - linebuf;
			cp = linebuf;
			break;
		case TTY_COOKED:
			/* Perform cooked-mode line editing */
#ifdef PC9801
			switch(c)
#else
			switch(c & 0x7f)
#endif
			{
				case '\015':	/* Terminal may generate either */
				case '\012':
					*cp++ = '\015';
					*cp++ = '\012';	/* guaranteed to bust array! K5JB */
					printf("\n");
					cnt = cp - linebuf;
#ifdef EDIT
					for(i=0;i<LINESIZE;i++){   /* save in lastline */
						if(linebuf[i] == '\012')	/* will use '\r' as marker */
							break;
						lastline[i] = linebuf[i];
					}
					editing = FALSE;
					ep = linebuf;
#endif
               cp = linebuf;
					break;

				case CTLU:
				case CTLY:	/* Line kill - also active with no echo */
					erase = TRUE;	/* borrow an existing int */
#ifdef EDIT
				case CTLE:	/* redisplay last line */
					if(editing && !erase)
						break;
#endif
					if(ttyecho) {
						while(cp != linebuf){
							cp--;
							printf("\b \b");
						}
					} else
						cp = linebuf;
					if(erase)
						break;
#ifdef EDIT
					if(ttyecho){	/* don't need to edit */
										/* when no echo */
						cp = linebuf;
						for(i=0;i<LINESIZE;i++){
							if(lastline[i] == '\015')
								break;
							linebuf[i] = lastline[i];
							putchar(linebuf[i]);
							cp++;
						}
						if(i){
							ep = &linebuf[i];
							editing = TRUE;
						}
					}
					break;

				case CTLD:	/* forward one char */
					if(editing && cp < ep){
						putchar(*cp++);
					}
					break;

				case CTLF:	/* forward one word */
					if(editing)
						while(cp < ep){
							putchar(*cp++);
							if(isspace(*cp)){ /* isspace() costs 258 bytes but it is*/
								if(cp < ep)		/* already used in smtpserv.c */
									putchar(*cp++);
								break;
							}
						}
					break;

				case CTLS:	/* backward one char */
					if (editing && cp != linebuf){
						printf("\b");
						cp--;
					}
					break;
#endif
				case RUBOUT:
				case '\b':		/* Backspace - note this works when no echo */
					if(cp != linebuf){
						cp--;
						printf("\b \b");	/* this isn't cool if no ttyecho, but */
					}
					break;
				case CTLR:	/* print line buffer */
					if(ttyecho) {
						printf("^R\n");
						rp = linebuf ;
						while (rp < cp)
							putchar(*rp++);
					}
					break ;
				case CTLV:
					ttymode = TTY_LIT;
					break;
				case CTLW:	/* erase word backward */
					erase = TRUE;
				case CTLA:	/* move cursor back one word */
					if(ttyecho){
						if(cp != linebuf && isspace(*(cp-1))){/* not at end of line */
							cp--;	/* back up onto the space */
							putchar('\b');
						}
						while (cp != linebuf) {
							cp--;
							if(erase)
								printf("\b \b") ;
							else
								printf("\b");
							if (isspace(*cp)) {
								putchar(*cp++);
								break;
							}
						}
					}
					break ;

				default:	/* Ordinary character */
					*cp++ = c;

				/* ^Z is a common screen clear character - K5JB */
					if (ttyecho && (c != CTLZ))
						putchar(c);

				/* if line is too long, we truncate it */
					if(cp > &linebuf[LINESIZE - 3]){	/* room for \r\n
														but NO null */
						linebuf[LINESIZE - 2] = '\015';
						linebuf[LINESIZE - 1] = '\012';
						cnt = LINESIZE;
						cp = linebuf;
					}
					break;
			}	/* switch ch in case TTY_COOKED */
	}	/* switch ttymode */
	if(cnt > 0)
		*buf = linebuf;
	else
		*buf = (char *)0;	/* K5JB */
#ifdef	FLOW
	if(cp > linebuf)
		ttyflow = 0;
	else
		ttyflow = 1;
#endif
	fflush(stdout);
	return cnt;
}
