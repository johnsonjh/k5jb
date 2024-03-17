/* configuration file for bm.  Put your favorite options in here */

#define	SETVBUF	/* Costs about 8k but speeds up file I/O 20-25% */
#undef	PRINTER	/* Enable for printer support */
/* DVREPORT is && with MSDOS in main.c */
#define	DVREPORT /* Do coreleft report for DESQview and Windows users */

#if defined(COH386) || defined(_OSK)
#undef SETVBUF
#endif

#undef	CLEAR	/* screen clearing stuff -- I HATE IT when it do dat! */
#ifdef UNIX
#undef CLEAR
#endif

#define SIGNALS	/* only catches interrupt (rubout) in Unix versions */

#ifdef MSDOS
#undef SIGNALS
#endif

#ifdef MSDOS
#undef VIDEO	/* direct video mode stuff */
#endif

/* set up the environmental variables you want to use, NOS may prefer something
 * else.  The Unix version will try $HOME before looking in current directory.
 * only MS-DOS uses root directory.
 */
#define HOMEDIR "NETHOME"	/* this is same as one used with NET.EXE */

/* If you want to change username when you change notefile, define this */
#define SWITCHUSER
/* To add quoting (">") to lines on imported messages, define this */
#define QUOTING

/* if you want to add the -a alias file option as an argument to select
 * a different alias file, define this.  If you use a non standard alias
 * file name NET (or NOS) won't be able to use it
 */
#undef OPTALIAS
