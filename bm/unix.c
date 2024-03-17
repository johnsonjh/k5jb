/* This file contains machine specific functions */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef COH386
#include <sys/termio.h>
#else
#include <termio.h>
#endif
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include "bm.h"
#include "ndir.h"

struct termio	savetty;
int savettyfl;
int	inrawmode = 0;		/* tell weather savetty is valid - also use as flag */

/* This function should put the tty in a mode such that single characters
* can be read without waiting for a complete line. Echo will be off.
*/
int
setrawmode()
{
	struct termio ttybuf;
	if(inrawmode)
		return(0);	/* already raw mode */
	ioctl(0, TCGETA, &ttybuf);
	savetty = ttybuf;
	ttybuf.c_lflag &= ~(ICANON|ECHO); /* will turn off echo also */
	/* note that the following blocking I/O is ineffective with older
	 * Coherent (COH386) but should work OK with 4.xx version (COH4) */
	/* This method is not necessary for BM but is a good practice anyhow */
	ttybuf.c_cc[VTIME] = 1;	/* 1/10 second, effective when ~ICANON */
	ttybuf.c_cc[VMIN] = 0; /* return if we get more than this */
	if ((savettyfl = fcntl(0, F_GETFL, 0)) == -1) {
		perror("Could not read console flags");
		return -1;
	}
#ifdef NONBLOCK
	fcntl(0, F_SETFL, savettyfl | O_NDELAY); /* non-blocking I/O */
#else
	fcntl(0, F_SETFL, savettyfl & ~O_NDELAY); /* blocking I/O */
#endif
	ioctl(0, TCSETAW, &ttybuf);
	inrawmode = 1;
	return(0);
}

/* This function should restore the tty modes back to cooked mode -
reworked to eliminate garbage characters - K5JB */
void
setcookedmode()
{
	if (inrawmode) {
		ioctl(0, TCSETAW, &savetty);	/* added wait for drain */
		fcntl(0, F_SETFL, savettyfl);	/* new - K5JB */
		inrawmode = 0;
	}
}

/* This function return one character from the keyboard. It will wait
* for a character to be input. This function will not echo the character.
*/
int
getch()
{
	char c;
	if(inrawmode)
		while(read(fileno(stdin), &c, 1) <= 0)
		;
	else{
		setrawmode();
		while(read(fileno(stdin), &c, 1) <= 0)
		;
		setcookedmode();
	}
	return (int)c;
}

/* This function show clear screen and put cursor at top of screen */
void
screen_clear()
{
}

void
setsignals()
{
	/* ignore interrupt (rubout) */
	signal(SIGINT,SIG_IGN);
	/* if you want to ignore quit (Ctrl-\) do it here */
}

/* wildcard filename lookup */
void
filedir(name, times, ret_str)
char	*name;
int	times;
char	*ret_str;
{
	static char	dname[128], fname[128];
	static DIR *dirp = NULL;
	struct direct *dp;
	struct stat sbuf;
	char	*cp, temp[128];

	/*
	 * Make sure that the NULL is there in case we don't find anything
	 */
	ret_str[0] = '\0';

	if (times == 0) {
		/* default a null name to *.* */
		if (name == NULL || *name == '\0')
			name = "*.*";
		/* split path into directory and filename */
		if ((cp = strrchr(name, '/')) == NULL) {
			strcpy(dname, ".");
			strcpy(fname, name);
		} else {
			strcpy(dname, name);
			dname[cp - name] = '\0';
			strcpy(fname, cp + 1);
			/* root directory */
			if (dname[0] == '\0')
				strcpy(dname, "/");
			/* trailing '/' */
			if (fname[0] == '\0')
				strcpy(fname, "*.*");
		}
		/* close directory left over from another call */
		if (dirp != NULL)
			closedir(dirp);
		/* open directory */
		if ((dirp = opendir(dname)) == NULL) {
			printf("Could not open DIR (%s)\n", dname);
			return;
		}
	} else {
		/* for people who don't check return values */
		if (dirp == NULL)
			return;
	}

	/* scan directory */
	while ((dp = readdir(dirp)) != NULL) {
		/* test for name match */
		if (wildmat(dp->d_name, fname)) {
			/* test for regular file */
			sprintf(temp, "%s/%s", dname, dp->d_name);
			if (stat(temp, &sbuf) < 0)
				continue;
			if ((sbuf.st_mode & S_IFMT) != S_IFREG)
				continue;
			strcpy(ret_str, dp->d_name);
			break;
		}
	}

	/* close directory if we hit the end */
	if (dp == NULL) {
		closedir(dirp);
		dirp = NULL;
	}
}


/*
**	strcmpic: string compare, ignore case
*/

stricmp(s1, s2)
char *s1, *s2;
{
	register char *u = s1;
	register char *p = s2;

	while(*p != '\0') {
		/* chars match or only case different */
		if(tolower(*u) == tolower(*p)) {
			p++;	/* examine next char */
			u++;
		} else {
			break;	/* no match - stop comparison */
		}
	}

	return(tolower(*u) - tolower(*p)); /* return "difference" */
}

#if defined(COH386) && !defined(COH4)
	/* this is a temporary work around because tmpfile() isn't working in
	 * earlier versions Coherent.  We are not removing the temporary
	 * files in bm, later Coherent works fine.
	 */
FILE *
tmpfile()
{
	char *buf,*tmpnam();
	FILE *fp;
#ifdef LATER	/* may come back later and do cleanup - K5JB */
	static int nr_tmpfiles;
	if((buf = (char *)malloc(L_tempnam)) == 0)
		return 0;
#else
	buf = tmpnam(NULL);
#endif
	if((fp = fopen(buf,"w+")) == NULL)
		return 0;
	return fp;
}
#endif
