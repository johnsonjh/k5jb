/* unixopt.h -- Unix specific options -- Moved here from Makefile
 * for dependency management purposes - K5JB
 */

/* If W5GFE is defined, net will cause incoming telnet data to display
in inverse video on terminals so equipped. Note this has been tested
with SCO Unix (native and GNU compilers) and with AT&T Sys V on the 3b2.
Apparently doesn't work with OSK.
*/

#define W5GFE
#ifdef _OSK
#undef W5GFE
#endif
#ifdef W5GFE
/*
Some terminals output a space when they change to STANDOUT mode.
If your screen has unexpected spaces in it, you might try using the
define STOOPID:
*/
#undef STOOPID

#endif	/* W5GFE */


/* if you have the mkdir() function, define the following: */
#define HAVEMKDIR

/* This is a wait value for the Coherent sleep2() function */
#define COHWAIT 50	/* 50 ms */

/* if you want remote users to be able to log on to your machine with
 * telnet - Requires having pseudo tty ports on the computer. This has
 * only been successfully tested with Coherent.
 */

#define TELUNIX
#ifdef _OSK
#undef TELUNIX
#endif
