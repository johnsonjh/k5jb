#ifdef	MSDOS
char *spool = "/spool";
char *mailspool = "mail";	/* Incoming mail */
char *mailqdir = "mqueue";		/* Outgoing mail spool */
char *alias = "alias";			/* alias file */
#endif

char timez[] = "xxxxxxxx";		/* K5JB */

#ifdef	UNIX
char *spool = "/usr/spool";
char *mailspool = "mail";
char *mailqdir = "mqueue";
char *alias = "aliases";	/* alias file, used only if */
#endif					/* there is no $NETHOME     */

#ifdef	AMIGA
char mailspool[] = "TCPIP:spool/mail";
char mailqdir[] = "TCPIP:spool/mqueue";
char alias[] = "TCPIP:alias";
#endif

#ifdef	MAC
char mailspool[] = "Mikes Hard Disk:spool:mail:";
char mailqdir[] = "Mikes Hard Disk:spool:mqueue:";
char alias[] = "Makes Hard Disk:alias";
#endif
