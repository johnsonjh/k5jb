#include <stdio.h>
#include <ctype.h>
#include "bm.h"
#include "header.h"

struct token hd[] = {
	"Status: ", STATUS,
	"Status: ", STATUS,
	"Received: ", RECEIVED,
	"From: ", FROM,
	"To: ", TO,
	"Date: ", DATE,
	"Message-Id: ", MSGID,
	"Subject: ", SUBJECT,
	"Reply-To: ", REPLYTO,
	"Sender: ", SENDER,
	NULLCHAR, 0
};

/* return the header token type */
int
htype(s)
char *s;
{
	register char *p;
	register struct token *hp;

	p = s;
	/* check to see if there is a ':' before and white space */
	while (*p != '\0' && *p != ' ' && *p != ':')
		p++;
	if (*p != ':')
		return NOHEADER;

	for (p = s, hp = hd; hp->str != NULLCHAR; hp++) {
		if (prefix(hp->str,p))
			return hp->type;
	}
	return UNKNOWN;
}

prefix(pref,full)
register char *pref, *full;
{
	register char fc, pc;

	while ((pc = *pref++) != '\0') {
		fc = *full++;
		if (isupper(fc))
			fc = tolower(fc);
		if (isupper(pc))
			pc = tolower(pc);
		if (fc != pc)
			return 0;
	}
	return 1;
}
