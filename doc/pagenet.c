/* pagenet.c -- another document formatting utility
 * Primarily used to make a print formatted copy of TCP/IP manual, but with
 * some restrictions can be used for other things.  Four switches, -m, -p,
 * -c and -s can be used.  -m6 adds 6 spaces to left margin.  (Maximum is 12).
 * -p10 starts first page with page 10.  (For formatting reasons, maximum
 * page should be less than 1000.  -cchapter_number adds chapter number and
 * dash before page number.  Chapter number must be less than 99. -sstring
 * sets the title to string.  This must be a continuous string, like
 * -sNew_Title, because no special attempt is made to extract from multiple
 * args.  Usage is given with unrecognized arg. Use redirection.  Pagenet.exe
 * uses stdin and stdout.  Joe, K5JB 7/4/91
 */
/* Wordstar note.  Use .rm 74 and then clean to prepare file.  There will
 * be hidden spaces at ends of lines so maximum -m value is 5 before it starts
 * subtracting lines for wordwrap.  (added space zap so -m could be 6, though
 * notes to be distributed will indicate to use 5.)
 */

#include <stdio.h>

#define LINELEN 128
#define LPP	66

errexit()
{
	printf("usage: pagenet [-cchap] [-pfirstpg] [-mmarg] [-ssubj] < infile > outfile\n");
	exit(1);
}

/* replace terminating end of line marker(s) and trailing spaces with null */
rip(s)
register char	*s;
{
register char *ps;
	ps = s;
	for (; *s; s++)
		if (*s == '\r' || *s == '\n') {
			*s = '\0';
			for(s--;s >= ps;s--)
				if(*s == ' ')
					*s = '\0';
					else
						break;
			break;
		}
}

main(argc,argv)
int argc;
char **argv;
{
char line[LINELEN];
int pagenr = 1, margin = 0;
int i, j, linenr,holdit,chapter = 0;
/* following is 79 spaces */
static char title[] = "                                                                               ";
char *deftitle = "TCP/IP Manual";
char *page = "Page ";

	if(argc > 1)
		for(i=1;i<argc;i++)
			if(*argv[i] == '-')
				switch (argv[i][1]){
					case 'p':
						pagenr = atoi(&argv[i][2]);
						break;
					case 'm':
						margin = atoi(&argv[i][2]);
						if(margin > 12)
							margin = 12;
						break;
					case 's':
						for(j=0;argv[i][2+j];j++)
							title[j] = argv[i][2+j];
						break;
					case 'c':
						chapter = atoi(&argv[i][2]);
						break;
					default:
						errexit();
					return;
				}
			else
				errexit();
	if(title[0] == ' ')
		for(j=0;j<13;j++)
			title[j] = deftitle[j];

/* leave room for 3 digit page number (if more than 9 chapters) or a 4 */
/* digit number and a space at right margin */
	for(j=75 - 5 - margin - (chapter == 0 ? 0 : 2);;j++,page++){
		title[j] = *page;
		if(!*page)
			break;
	}
	if(chapter){
		if(chapter > 9)
			title[j++] = (char) (0x30+chapter/10);
		title[j++] = (char) (0x30+chapter %10);
		title[j++] = '-';
		title[j] = '\0';
	}
	linenr = LPP - 4;
	holdit = 0;
	for(;;){
		if(!holdit)
			if(fgets(line,LINELEN,stdin) == NULL)
				break;
		if(!strncmp(".rm",line,3))
			continue; 	/* Skip formatting command */
		if(linenr-- == LPP - 4)
			printf("\n\n\n\n");       /* fine tune this on Epson at home */
		rip(line);
		if(strlen(line) + margin > 81)	/* hope this doesn't happen but */
			linenr--;		/* if it does, this will fix small errors */
		if((holdit = strncmp(".pa",line,3)) == 0 || linenr < 7){ /* was 6 */
			while(linenr-- > 4)
				printf("\n");
			for(i=margin;i>0;i--)
				printf(" ");
			printf("%s%d\n",title,pagenr++);

/*
"TCP/IP Manual                                                     Page %d\n",pagenr++);
*/
			printf("\n\n\n\n");

			linenr = LPP - 4;
		}else{
			holdit = 0;
			for(i=margin;i>0;i--)
				printf(" ");
			printf("%s\n",line);
		}
	}
	/* cleanup end of file if .pa not included */
	if(linenr-- != LPP - 4){
		while(linenr-- > 4)
			printf("\n");
		for(i=margin;i>0;i--)
			printf(" ");
		printf("%s%d\n",title,pagenr++);
		printf("\n\n\n\n");
	}
}
