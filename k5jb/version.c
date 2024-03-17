/* Change only the part after the decimal when making local changes.
 * This is pretty complex because I compile for a bunch who want custom
 * versions.  Unix/Coherent and MS-DOS version.c are different.
 * This is for Unix/Coherent
 */
#include "options.h"
#include "config.h"

char versionf[] = "K5JB.k33";	/* for FTP and the mbox's use */
void
version()
{
	printf("        This version is %s with options:\n        ",versionf);

#ifdef AX25
#ifdef SEGMENT
	printf("AX.25/SEG");
#else
	printf("AX.25");
#endif
#ifdef NETROM
	printf(", Netrom");
#endif
#ifdef AXMBX
	printf(", MBOX");
#endif
#else /* AX25 */
	printf("SLIP");
#endif

#ifdef KPCPORT
	printf(", KPC4");
#endif
#ifdef MULPORT
	printf(", Mulport");
#endif

#ifdef VCKT
	printf(", VC");
#endif
#ifdef AX25_HEARD
	printf(", Heard");
#endif
#ifdef COH386
	printf(", Coherent");
#else
#ifdef UNIX
	printf(", Unix");
#endif
#ifdef _OSK
	printf(", OS9-68K");
#endif
#endif
	printf("\n\n");
}
