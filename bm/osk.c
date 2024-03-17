/* OS- and machine-dependent stuff for OSK machines */
/* Note that I added a couple of type declarations, but otherwise didn't
 * touch this file - K5JB
 */

#include <stdio.h>
#include <sgstat.h>
#include <modes.h>
#include <dir.h>
#include <strings.h>
#include <ctype.h>

#define NULLCHAR (char *)0
#define NULLFILE (FILE *)0

#define MAXCMD 80
struct sgbuf orgsgbufi;
int inrawmode=0;

#ifndef	_UCC
#define MAXTMP 26
struct FILREC {
	FILE *filptr;
	char tmpnam[32];
};

struct FILREC tmpfils[MAXTMP];
#endif

/* Called at startup time to set up console I/O, memory heap */
int setrawmode()
{
	char termname[40];
	struct sgbuf sbuf;
	int i;
	
#ifndef	_UCC 
	for (i=0;i<MAXTMP;i++)
		tmpfils[i].filptr=NULL;
#endif
	_gs_opt(0,&sbuf);
	memcpy(&orgsgbufi,&sbuf,sizeof(sbuf));
    sbuf.sg_case = 0;
    sbuf.sg_backsp = 0;
    sbuf.sg_delete = 0;
    sbuf.sg_echo = 0;
    sbuf.sg_alf = 1;
    sbuf.sg_nulls = 0;
    sbuf.sg_pause = 0;
    sbuf.sg_page = 0;
    sbuf.sg_bspch = 0;
    sbuf.sg_dlnch = 0;
    sbuf.sg_eorch = 0;
    sbuf.sg_eofch = 0;
    sbuf.sg_rlnch = 0;
    sbuf.sg_dulnch = 0;
    sbuf.sg_psch = 0;
    sbuf.sg_bsech = 0;
    sbuf.sg_bellch = 0;
    sbuf.sg_xon = 0;
    sbuf.sg_xoff = 0;
    sbuf.sg_tabcr = 0;
    sbuf.sg_tabsiz = 0;
	sbuf.sg_kbich = 0;
	sbuf.sg_kbach = 0;
	_ss_opt(0,&sbuf);

	inrawmode=1;
	setbuf(stdin,NULL);
	return 0;
}

/* Called just before exiting to restore console state */
void
setcookedmode()
{
	if (inrawmode)
	{
	_ss_opt(0,&orgsgbufi);
	inrawmode=0;
	}
}

/* dirutil.c
 *        OSK directory reading stuff.
 *        Matthias Bohlen
 *        Copyright 1987 Matthias Bohlen, All Rights Reserved.
 *        Permission granted for non-commercial copying and use, provided
 *        this notice is retained.
 */

static DIR *d;
static struct direct *r;


/* wildcard filename lookup */
void
filedir (dirname, times, ret_str)
char *dirname;
int times;
char *ret_str;
{
	register int masklen;
	register char *cp;

	char dname[80], filemask[80];
	
	for (cp=dirname+strlen(dirname); cp>dirname && *--cp != '/'; )
	  ;
	strncpy(dname, dirname, cp-dirname);
	dname[cp-dirname] = '\0';
	strcpy(filemask, cp+1);
	
    masklen = strlen(filemask);
    
	if (times==0) {   /* first time */
		if ( (d=opendir(dname)) == NULL ) {
			*ret_str = '\0';
			return;
		}
	}
	
	while (1) {
		if ( (r=readdir(d)) == NULL ) {
			*ret_str = '\0';
			closedir(d);
			return;
		}
		if (_cmpnam(r->d_name, filemask, masklen) == 0)
			if (r->d_name[0] != '.') {
				strcpy(ret_str, r->d_name);
				return;
			}
	}
}

doshell(argc,argv)
int argc;
char **argv;
{
	struct sgbuf sgbufi;
	_gs_opt(0,&sgbufi);
	_ss_opt(0,&orgsgbufi);
	system("");
	_ss_opt(0,&sgbufi);
}

/* This function is supposed to return one character from the keyboard
 * waiting until a key is pressed, but with the input in cooked mode
 * it may not return until a new line is entered.  In Unix I had to
 * twiddle with the input to get it to return with just one character.
 * This function is just so we can do the "hit any key to continue" and
 * the wordwrap thing.  It should not echo the keypress.  K5JB
*/
int
getch()
{
	char c;
	read(0,&c,1);
/*	
	c = getchar();
*/
	return c;
}


void
setsignals()
{
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


char *
make_path(path_ptr,file_ptr,slash)
char *path_ptr;
char *file_ptr;
int slash;
{
	char *cp, *malloc();

	if ((cp = malloc(strlen(path_ptr) + strlen(file_ptr) + 2)) == NULLCHAR)
		perror("malloc");
	else {
		sprintf(cp, "%s%s%s", path_ptr,slash ? "/" : "",file_ptr);
		return(cp);
	}
	return(NULLCHAR);
}



/***************************************************************************/
/*                                                                         */
/* _secsince70() : Unix library (OS9/68000)                                */
/* =============                                                           */
/*                                                                         */
/* Author:     K. Schmitt                                                  */
/* Compiler:   Microware C Vers. 3.0                                       */
/* OS:         OS9/68000 Vers. 2.2                                         */
/*                                                                         */
/* Edition History                                                         */
/* ===============                                                         */
/*                                                                         */
/* Ed. 0.00  Date 11/16/88                                                 */
/*           First version                                                 */
/*                                                                         */
/***************************************************************************/
/*                                                                         */
/* Description                                                             */
/*                                                                         */
/*     #include <time.h>
       time_t _secsince70 (date)
       char date[];

       returns # of secs past since 01/01/1970 GMT

*/

#include <time.h>


time_t _secsince70 (date)
          char date []; /* yy/mm/dd/hh/mm/ss */
                        /*  0  1  2  3  4  5 */
          {
          struct tm time70, time_;
          time_t mktime();


          time70.tm_sec   = 0;
          time70.tm_min   = 0;
          time70.tm_hour  = 0;
          time70.tm_mday  = 1;
          time70.tm_mon   = 0;
          time70.tm_year  = 70;
          time70.tm_isdst = -1;
          time_.tm_sec   = (int) date[5];
          time_.tm_min   = (int) date[4];
          time_.tm_hour  = (int) date[3];
          time_.tm_mday  = (int) date[2];
          time_.tm_mon   = (int) date[1] - 1;
          time_.tm_year  = (int) date[0];
          time_.tm_isdst = -1;
          return (mktime(&time_.tm_sec) - mktime(&time70.tm_sec));
          
          } /* end of _secsince70 */

/***************************************************************************/
/*                                                                         */
/* stat() : Unix library (OS9/68000)                                       */
/* ======                                                                  */
/*                                                                         */
/* Author:     K. Schmitt                                                  */
/* Compiler:   Microware C Vers. 3.0                                       */
/* OS:         OS9/68000 Vers. 2.2                                         */
/*                                                                         */
/* Edition History                                                         */
/* ===============                                                         */
/*                                                                         */
/* Ed. 0.00  Date 11/16/88                                                 */
/*           First version                                                 */
/*                                                                         */
/***************************************************************************/
/*                                                                         */
/* Description                                                             */
/*                                                                         */
/*

     NAME  
          stat, fstat - get file status

     SYNOPSIS
          #include <UNIX/stat.h>

          int stat (path, buf)
          char *path;  
          struct stat *buf;

          int fstat (fildes, buf)  
          int fildes;  
          struct stat *buf;

     DESCRIPTION
          Path points to a path name naming a file.  Read, write or
          execute permission of the named file is not required, but
          all directories listed in the path name leading to the file  
          must be searchable.  Stat obtains information about the  
          named file.  

          Fstat obtains information about an open file known by the
          file descriptor fildes, obtained from a successful open, 
          creat, dup, fcntl, or pipe system call.  

          Buf is a pointer to a stat structure into which information
          is placed concerning the file.

          The contents of the structure pointed to by buf include the
          following members:

           ushort     st_mode;     File mode
           ino_t      st_ino;      Inode number
           dev_t      st_dev;      ID of device containing a directory
                                   entry for this file
           dev_t      st_rdev;     ID of device. This entry is defined
                                   only for character special or block
                                   special files
           short      st_nlink;    Number of links
           ushort     st_uid;      User ID of the file's owner
           ushort     st_gid;      Group ID of the file's group
           off_t      st_size;     File size in bytes
           time_t     st_atime;    Time of last access
           time_t     st_mtime;    Time of last data modification
           time_t     st_ctime;    Time of last file status change
                                   Times measured in seconds since
                                   00:00:00 GMT, Jan. 1, 1970

          The following parameters are affected by different system
          calls:

          st_atime  Time when file data was last accessed.
                    Changed by the following system calls:  creat,
                    mknod, pipe, utime, and read.

          st_mtime  Time when data was last modified.
                    Changed by the following system calls:  creat,
                    mknod, pipe, utime, and write.

          st_ctime  Time when file status was last changed.
                    Changed by the following system calls:  chmod,
                    chown, creat, link, mknod, pipe, unlink, utime,
                    and write.

          Stat will fail if one or more of the following are true:

          [ENOTDIR]      A component of the path prefix is not a
                         directory.

          [ENOENT]       The named file does not exist.

          [EACCES]       Search permission is denied for a component
                         of the path prefix.

          [EFAULT]       Buf or path points to an invalid address.

          Fstat will fail if one or more of the following are true:

          [EBADF]        Fildes is not a valid open file descriptor.

          [EFAULT]       Buf points to an invalid address.

     RETURN VALUE
          Upon successful completion a value of 0 is returned.
          Otherwise, a value of -1 is returned and errno is set to
          indicate the error.

*/

/*
 * UNIX stat.h
 */

#ifndef CLK_TCK
#include <time.h>
#endif

typedef char * dev_t;  /* pointer to the device name */

/*
 * file control structure
 */

struct stat
     {
     unsigned short  st_mode;
     unsigned short  st_nlink;
     unsigned short  st_uid;
     unsigned short  st_gid;
     long            st_size;
     time_t          st_atime;
     time_t          st_mtime;
     time_t          st_ctime;
     long            st_ino;
     dev_t           st_dev;
     dev_t           st_rdev;
     } ;

#ifndef S_IREAD
#include <modes.h>
#endif

#include <direct.h>

extern time_t _secsince70 ();

int stat (fname, buf)
          char fname [];
          struct stat *buf;
          {
          int fd, ret;
          DIR *dirp;

          if ((fd = open (fname, S_IREAD)) == -1)
               {
               if ((dirp = opendir (fname)) == 0)
                    return (-1);
               fd = dirp -> dd_fd;
               }
          else
               {
               dirp = 0;
               }

          ret = fstat (fd, buf);
          if (dirp == 0) close (fd);
          else           closedir (dirp);

          return (ret);

          } /* end of stat */



int fstat (fd, buf)
          int fd;
          register struct stat *buf;
          {
          struct fildes fildes;
          register int i;
          register char *p;
          char *malloc();

          if (_gs_gfd (fd, &fildes, sizeof (struct fildes)) == -1)
               return (-1);

          buf -> st_uid = (unsigned short) (fildes.fd_own [0]);
          buf -> st_gid = (unsigned short) (fildes.fd_own [1]);
          buf -> st_mode = (unsigned short) (fildes.fd_att) & 0377;
          buf -> st_nlink = (unsigned short) (fildes.fd_link);

          for (i = 0, p = (char *) (&buf -> st_size); i < 4; i++)
               {
               *p++ = fildes.fd_fsize [i];
               }

          buf -> st_atime = buf -> st_mtime = _secsince70 (fildes.fd_date);
          fildes.fd_dcr [3] = 0;
          fildes.fd_dcr [4] = 0;
          buf -> st_ctime = _secsince70 (fildes.fd_dcr);
          if ((buf -> st_rdev = buf -> st_dev = malloc(30)) != (char *) 0)
               {
               _gs_devn (fd, buf -> st_dev);
               }

          return (0);

          } /* end of fstat */


/***************************************************************************/
/*                                                                         */
/* isatty() : Unix library (OS9/68000)                                     */
/* ========                                                                */
/*                                                                         */
/* Author:     K. Schmitt                                                  */
/* Compiler:   Microware C Vers. 3.0                                       */
/* OS:         OS9/68000 Vers. 2.2                                         */
/*                                                                         */
/* Edition History                                                         */
/* ===============                                                         */
/*                                                                         */
/* Ed. 0.00  Date 11/11/88                                                 */
/*           First version                                                 */
/*                                                                         */
/***************************************************************************/
/*                                                                         */
/* Description                                                             */
/*                                                                         */
/*

     NAME
          isatty - check fildes

     SYNOPSIS
          int isatty (fildes)
          int fildes;
          struct sgbuf _tty_opt_; (GLOBAL defined - see ioctl() )

     DESCRIPTION
          Isatty returns 1 if fildes is associated with a terminal
          device, 0 otherwise. If the fildes is invalid, -1.

     FILES sgstat.h

*/

#define DT_SCF      0    /* device type: SCF        */
#define ERROR      -1


struct sgbuf _tty_opt_;  /* buffer for path options */

isatty(fildes)
          int fildes;
          {
          extern int _gs_opt();
          register struct sgbuf *tty_options = &_tty_opt_;

          if (_gs_opt(fildes,tty_options) == ERROR) return (ERROR);
          if (tty_options->sg_class != DT_SCF) return (0);

          return (1); /* it's a tty */

          } /* end of isatty */

#ifndef	_UCC

FILE *tmpfile()
{
	char *ep;
	char fname[80];
	int done=0;
	int indx;
	char *make_path();
	char *tmp;
	extern char *getenv();
	FILE *fil, *fopen();
	int access();
		
	fname[0]='/';
	fname[1]='a';
	
	if ((ep = getenv("TMPDIR")) == NULLCHAR)
			if ((ep = getenv("NETSPOOL")) == NULLCHAR)
				perror("must set NETSPOOL env var\n");
	done=0;
	indx=0;
	while (fname[1]<'z' && !done)
	{   fname[2]='\0';
		
		strcat(fname,".net_tmp");
		tmp = make_path(ep,fname,0);
		if	(!access(tmp,0))
		{	indx=indx+1;
			fname[1]=fname[1]+1;
			free(tmp);
		}
		else
		{
			if ((fil=fopen(tmp,"w+"))!=NULL)
			{
				tmpfils[indx].filptr=fil;
				strcpy(tmpfils[indx].tmpnam,tmp);
				done=1;
			}
			else
			{
				indx=indx+1;
				fname[1]=fname[1]+1;
			}
		}
	}
	if (done)
	{
		free(tmp);
		return fil;
	}
	else
		return NULLFILE;
}

int tmpclose(t)
FILE *t;
{
	int i=0;
	
	while(i<26)
	{
		if (tmpfils[i].filptr==t)
		{   
			fclose(t);
			unlink(tmpfils[i].tmpnam);
			tmpfils[i].filptr=NULL;
	 		return 0;
		}
		i++;
	}
/*	printf("attempted to delete non-tempfile\n"); unavoidable but not important */
	fclose(t);
	return 0;

}	
/***************************************************************************/
/*                                                                         */
/* strpbrk() : Unix library (OS9/68000)                                    */
/* ========                                                                */
/*                                                                         */
/* Author:     K. Schmitt                                                  */
/* Compiler:   Microware C Vers. 3.0                                       */
/* OS:         OS9/68000 Vers. 2.2                                         */
/*                                                                         */
/* Edition History                                                         */
/* ===============                                                         */
/*                                                                         */
/* Ed. 0.00  Date 11/15/88                                                 */
/*           First version                                                 */
/*                                                                         */
/***************************************************************************/
/*                                                                         */
/* Description                                                             */
/*                                                                         */
/*

          char *strpbrk (s1, s2)
          char *s1, *s2;

          Strpbrk returns a pointer to the first occurrence in string  
          s1 of any character from string s2, or NULL if no character  
          from s2 exists in s1.
*/

char *strpbrk (s1, s2)
          register char *s1, *s2;
          {
          register char *p;

          while (*s1)
               {
               p = s2;
               while (*p)
                    {
                    if (*p++ == *s1) return (s1);
                    }
               s1++;
               }

          return ((char *) 0);

          } /* end of strpbrk */


/***************************************************************************/
/*                                                                         */
/* perror() : Unix library (OS9/68000)                                     */
/* ========                                                                */
/*                                                                         */
/* Author:     K. Schmitt / Ch. Engel                                      */
/* Compiler:   Microware C Vers. 3.0                                       */
/* OS:         OS9/68000 Vers. 2.2                                         */
/*                                                                         */
/* Edition History                                                         */
/* ===============                                                         */
/*                                                                         */
/* Ed. 0.00  Date 11/11/88                                                 */
/*           First version                                                 */
/*                                                                         */
/***************************************************************************/
/*                                                                         */
/* Description                                                             */
/*                                                                         */
/*

     NAME
          perror, sys_errlist, sys_nerr, errno - system error messages

     SYNOPSIS
          perror (s)
          char *s;

          int sys_nerr;
          char *sys_errlist[ ];

          int errno;

     DESCRIPTION
          Perror produces a short error message on the standard error,
          describing the last error encountered during a system call
          from a C program.  First the argument string s is printed,
          then a colon, then the message and a new-line.  To be of
          most use, the argument string should be the name of the
          program that incurred the error.  The error number is taken
          from the external variable errno, which is set when errors
          occur and cleared when non-erroneous calls are made.

          To simplify variant formatting of messages, the vector of
          message strings sys_errlist is provided; errno can be used
          as an index in this table to get the message string without
          the new-line.  Sys_nerr is the largest message number
          provided for in the table; it should be checked because new
          error codes may be added to the system before they are added
          to the table.

*/


#define YES  1
#define NO   0
#define OK   0
#define ERR  -1

#define ERRFILE "/dd/SYS/errmsg"
#define MAXERR  256

extern int errno;

int sys_nerr = MAXERR;
char *sys_errlist [MAXERR];
static int errstat = OK;

char *malloc ();

perror (s)
          char *s;
          {
          static int first = YES;
          FILE *errfile;

          if (first)
               {
               first = NO;
               if ((errfile = fopen (ERRFILE, "r")) == NULL)
                    {
                    errstat = ERR;
                    }
               else
                    {
                    _build_errlist (errfile);
                    fclose (errfile);
                    }
               }

          if (errstat)
               fprintf (stderr, "%s: %03d:%03d\n",
                    s, errno >> 8, errno & 0x0ff);
          else
               fprintf (stderr, "%s: %s\n", s, sys_errlist [errno&(MAXERR-1)]);

          } /* end of perror */

#ifdef STATIC
static
#endif
_build_errlist (errfile)
          FILE *errfile;
          {
          char message [1024], line [80];
          register unsigned i;
          register char *p;

          for (i = 0; i < MAXERR ; i++)
               sys_errlist [i] = NULL;

          for (fgets (line, 80, errfile); ! feof (errfile); )
               {
               if (! isdigit (line [0]) || strlen (line) < 7)
                    {
                    fgets (line, 80, errfile);
                    continue;
                    }

               i = (atoi (line) << 8) | atoi (line + 4);
               for (p = line + 7; *p && isspace (*p); p++)
                    ;

               strcpy (message, p);
               fgets (line, 80, errfile);
               for ( ; line [0] != '\0' && ! isdigit (line [0]);
                    fgets (line, 80, errfile))
                    {
                    for (p = line; *p && isspace (*p); p++)
                         ;

                    strcat (message, p);
                    }

               if (i >= MAXERR) break;

               if ((sys_errlist [i] = malloc (strlen (message) + 1)) ==
                    NULL)
                    {
                    errstat = ERR;
                    return;
                    }
               else
                    {
                    strcpy (sys_errlist [i], message);
                    }
               }
          } /* end of _build_errlist */

#endif

/* E O F */
