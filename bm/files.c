/* look in config.h for some manifest constants */
#include "config.h"

#ifdef	MSDOS
char *spool = "/spool";
char *mailspool = "mail";	/* Incoming mail */
char *mailqdir = "mqueue";		/* Outgoing mail spool */
#ifdef OPTALIAS
char *default_alias = "alias";			/* alias file */
#else
char *alias = "alias";			/* alias file */
#endif
char *default_runcom = "bm.rc";	/* runtime configuration file */
#endif

char timez[] = "xxxxxxxx";		/* K5JB */

#ifdef	UNIX
char *spool = "/usr/spool";
char *mailspool = "mail";
char *mailqdir = "mqueue";
#ifdef OPTALIAS
char *default_alias = "aliases";
#else
char *alias = "aliases";
#endif
char *default_runcom = ".bmrc";
#endif

#ifdef	AMIGA
char mailspool[] = "TCPIP:spool/mail"; /* this doesn't look like it would */
char mailqdir[] = "TCPIP:spool/mqueue";/* work, but what do I know about */
#ifdef OPTALIAS                        /* amiga -- K5JB */
char default_alias[] = "TCPIP:alias";
#else
char alias[] = "TCPIP:alias";
#endif
char default_runcom[] = "TCPIP:bmrc"; /* I guess, K5JB */
#endif
