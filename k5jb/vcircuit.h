/* vcircuit.h
 * Virtual Circuit trickery - previously called rose.h - K5JB
 * Modified to make it simpler and deal with any station we want to
 * do VC business with
 */

/* Virtual circuit table structure */
typedef struct {
	struct ax25_addr ipcall;	/* other IP station's ax25 call */
#ifdef NOTNEEDED
	char port[5];	/* port where this neighbor is found */
#endif
} vcircuittab;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
