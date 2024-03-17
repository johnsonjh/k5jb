/* options.h contains more commonly changed options.  config.h needs to know
 * if DRSI or KPCPORT are defined so options.h is included in config.h.
 * Note that only KPCPORT, VCKT, AX25_HEARD & SHELL apply to Unix.  See also
 * unixopt.h for Unix.
*/

/* These first ones are for MS-DOS only! But, clumsy me, I forget... */
#ifdef MSDOS
#undef	DRSI /* DRSI standard driver, also alter config.inc + 8004 bytes */
/* Note, combios is preliminary and still evolving */
#undef	COMBIOS /* Combios/mbbios/xxxbios interface - adds 1764 bytes */
/* For packet driver, define PACKET.  If you want to do ethernet, 
 * Also alter config.h.  This packet driver has only be tested with the
 * G8BPQ scheme.  Should work with Baycom.  Adds 2552 bytes */
#undef	PACKET
#undef SERIALTEST /* SERIALTEST only applies to MS-DOS; counts rx overruns
			on serial port.  Costs 200 bytes */
#endif /* MSDOS */

/* MS-DOS doesn't do the shell escape well, use another DV window.
 * This advice applys to multitasking OSs too.  During shell escapes
 * NET is dead as a doornail.
 */
#define SHELL

#ifdef MSDOS
#undef SHELL
#endif

/* The following are for all operating systems */

#define	KPCPORT	/* Kantronics KPC port switch - adds 1084 bytes N5OWK */
#define	MULPORT /* Grapes Multiport code - adds 1412 bytes */
#define	VCKT /* Enable trapping spurious ROSE frames - adds 1774 bytes */
#define	AX25_HEARD /* Heard list - adds 3716 bytes */
#undef	CPUSTAT /* CPUSTAT is a diagnostic, adds 760 bytes */
#define	CUTE_FTP /* Attempt to interpret ftp login sequence - adds 270 bytes */
		/* Undefine if you deal with NOS boogers a lot */

/* #define SOKNAME Show names in hosts.net on log and tcp stat - moved to
 * sokname.h
 * #define TIMEMARK Timestamp trace - see trace.c - moved to trace.h
 */


/* Theis was added with k32-33.  It adds an ax25 segment command
 */
#define SEG_CMD
