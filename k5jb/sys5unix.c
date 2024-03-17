/*
	FILE: unix.c
	
	Routines: This file contains the following routines:
		fileinit()   (moved this to main.c so MS-DOS could use it)
		sysreset()
		eihalt()
		kbread()
		clksec()
		tmpfile()	 for  Coherent - K5JB 
		restore()
		stxrdy()
		disable()
		memstat()
		filedir()
		check_time()
		getds()
		audit()
		doshell()
		dodir()
		rename()
		docd()
		ether_dump()
		mkdir()	 for those who don't have it 
		rmdir()
		
	Written by Mikel Matthews, N9DVG
	SYS5 stuff added by Jere Sandidge, K4FUM
K5JB: Added some new mbox stuff that permits reading mail and files.

*/
#ifndef _OSK
#include <stdio.h>
#include <signal.h>
#include <termio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <time.h>
#include <fcntl.h>		/* K5JB */

#include "global.h"
#include "cmdparse.h"
#include "iface.h"
#include "unix.h"
#include "unixopt.h"
#include "ndir.h"

#define	MAXCMD	1024

int asy_attach();

extern struct cmds attab[];
extern struct termio savecon;

unsigned nasy;

/* action routine for remote reset */
void
sysreset()
{
	extern int exitval;
	int doexit();	/* in main.c */

	exitval = 3;
	doexit();
#ifdef NOGOOD
	extern char *netexe;

	execl(netexe,netexe,0);
	execl("net","net",0);
	printf("reset failed: exiting\n");
	exit();
#endif
}

#ifdef NOTUSED
void
eihalt()
{
	tnix_scan();
}
#endif

int
kbread()
{
	unsigned char c;
#ifdef COH386
	int sleep2();

	sleep2(COHWAIT);	/* only needed when no async ports are used */
#endif
	if (read(fileno(stdin), &c, 1) <= 0)
		return -1;

	return ((int) c);
}

int
clksec()
{
	long tim;
	(void) time(&tim);

	return ((int) tim);
}

#ifdef COH386
FILE *
tmpfile()
{
	char *buf;
	FILE *fp;
#ifdef LATER	/* may come back later and do cleanup - K5JB */
	static int nr_tmpfiles;
	if((buf = (char *)malloc(L_tempnam)) == NULLCHAR)
		return 0;
#else
	buf = tmpnam(NULLCHAR);
#endif
	if((fp = fopen(buf,"w+")) == NULLFILE)
		return 0;
	return fp;
}
#endif

void
restore()
{
}

int
stxrdy()
{
	return 1;
}

int
disable()
{
	return 1;	/*  dummy function -- not used in Unix */
}

int
memstat()
{
	return 0;
}

#define NULLDIR (DIR *)0
#define NULLDIRECT (struct direct *)0


/* wildcard filename lookup */
void
filedir(name, times, ret_str)
char	*name;
int	times;
char	*ret_str;
{
	static char	dname[128], fname[128];
	static DIR *dirp = NULLDIR;
	struct direct *dp;
	struct stat sbuf;
	char	*cp, temp[128];

	/*
	 * Make sure that the NULL is there in case we don't find anything
	 */
	ret_str[0] = '\0';

	if (times == 0) {
		/* default a null name to *.* */
		if (name == NULLCHAR || *name == '\0')
			name = "*.*";
		/* split path into directory and filename */
		if ((cp = strrchr(name, '/')) == NULLCHAR) {
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
		if (dirp != NULLDIR)
			closedir(dirp);
		/* open directory */
		if ((dirp = opendir(dname)) == NULLDIR) {
			printf("Could not open DIR (%s)\n", dname);
			return;
		}
	} else {
		/* for people who don't check return values */
		if (dirp == NULLDIR)
			return;
	}

	/* scan directory */
	while ((dp = readdir(dirp)) != NULLDIRECT) {
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
	if (dp == NULLDIRECT) {
		closedir(dirp);
		dirp = NULLDIR;
	}
}


/* checks the time then ticks and updates ISS */
void
check_time()
{
	int32 iss();
	void tickle();
#ifdef COH386
	long time();
#else
	long times();
#endif

	struct tms tb;
	static long clkval;
	long ntime, offset;

	/* read elapsed real time (typ. 60 Hz) */
/* until I figure out how to make tick() work, will work in seconds */
#ifdef COH386
	time(&ntime);
#else
	ntime = times(&tb);
#endif

	/* resynchronize if the error is large (10 seconds or more) */
	offset = ntime - clkval;
	if (offset > (10000/MSPTICK) || offset < 0)
		clkval = ntime;

	/* Handle possibility of several missed ticks */
	while (ntime != clkval) {
		++clkval;
		icmpclk();
		tickle();
		(void) iss();
	}
}

int
getds()
{
	return 0;
}

void
audit()
{
}

int
doshell(argc, argv)		/* modified to make shell "stick" K5JB */
char	**argv;
{

	int	i, retn;
	char	str[MAXCMD];
	char	*cp;
	struct termio tt_config;
	int saverunflg;

	char	*getenv();

	str[0] = '\0';
	for (i = 1; i < argc; i++) {
		strcat(str, argv[i]);
		strcat(str, " ");
	}

	ioctl(0, TCGETA, &tt_config);
	ioctl(0, TCSETAW, &savecon);
	if ((saverunflg = fcntl(0, F_GETFL, 0)) == -1) {
		perror("Could not read console flags");
		return -1;
	}
	fcntl(0, F_SETFL, saverunflg & ~O_NDELAY); /* blocking I/O */

	if (argc > 1)
		retn = system(str);
	else if ((cp = getenv("SHELL")) != NULLCHAR && *cp != '\0')
		retn = system(cp);
	else
		retn = system("exec /bin/sh");

	fcntl(0, F_SETFL, saverunflg); /* restore non-blocking I/O, if so */
	ioctl(0, TCSETAW, &tt_config);
	printf("Returning you to net.\n");
	return retn;

}

int
dodir(argc, argv)
int	argc;
char	**argv;
{
	int	i, stat;
	char	str[MAXCMD];
	struct termio tt_config;

	strcpy(str, "ls -l ");
	for (i = 1; i < argc; i++) {
		strcat(str, argv[i]);
		strcat(str, " ");
	}

	ioctl(0, TCGETA, &tt_config);
	ioctl(0, TCSETAW, &savecon);

	stat = system(str);

	ioctl(0, TCSETAW, &tt_config);

	return stat;
}

int
rename(s1, s2)
char *s1, *s2;
{
	char tmp[MAXCMD];

	(void) sprintf(tmp, "mv %s %s", s1, s2);
	return(system(tmp));
}


/* enabled this command with k28 -- it was incomplete.  Tried to do it
in most portable way, though in SysV, it would be appropriate to feed
getcwd a null pointer and free the memory pointed to by getcwd's return
value.  This command is used for "pwd" and "cd".  - K5JB
*/
int
docd(argc, argv)
int argc;
char **argv;
{
	static char tmp[MAXCMD];
	char *getcwd();
	extern char *homedir;

	if(*argv[0] == 'c')
		if(chdir(argc > 1 ? argv[1] : homedir) == -1){
			printf("Can't change directory\n");
			return 1;
		}

	if (getcwd(tmp, sizeof(tmp) - 2) != NULL)
		printf("%s\n", tmp);

	return 0;
}

void
ether_dump()
{
}

#ifndef HAVEMKDIR
int
mkdir(s, m)
char	*s;
int	m;
{
	char tmp[MAXCMD];

	sprintf(tmp, "mkdir %s", s);
	if (system(tmp))
		return -1;
	if (chmod(s, m) < 0)
		return -1;

	return 0;
}
#endif

int
rmdir(s)
char	*s;
{
	char tmp[MAXCMD];

	sprintf(tmp, "rmdir %s", s);
	if (system(tmp))
		return -1;

	return 0;
}
#endif
